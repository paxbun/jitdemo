// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

// Check for unsupported platforms
#if _WIN32
#    if !(defined(_M_AMD64) && defined(_WIN64))
#        error this platform is not supported
#    endif
#else
#    if !defined(__x86_64__)
#        error this platform is not supported
#    endif
#endif

#include <array>
#include <bitset>
#include <cmath>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <vector>

export module jitdemo.jit.compile;

import jitdemo.expr.expression;
import jitdemo.expr.expression_tree_function;
import jitdemo.expr.function;
import jitdemo.expr.expressions;
import jitdemo.jit.compiled_function;

using ::jitdemo::expr::Expression;
using ::jitdemo::expr::ExpressionTreeFunction;
using ::jitdemo::expr::Function;
using ::jitdemo::expr::expressions::BinaryExpression;
using ::jitdemo::expr::expressions::BinaryExpressionOps;
using ::jitdemo::expr::expressions::ConstantExpression;
using ::jitdemo::expr::expressions::FunctionExpression;
using ::jitdemo::expr::expressions::VariableExpression;

export namespace jitdemo::jit
{

::std::shared_ptr<CompiledFunction>
Compile(::std::shared_ptr<ExpressionTreeFunction> const& function);

}

module : private;

using ::jitdemo::jit::CompiledFunction;

namespace
{

template <typename From, typename To>
To Transmute(From from)
{
    union
    {
        From from;
        To   to;
    } u {};
    u.from = from;
    return u.to;
}

enum class FloatRegister
{
    Xmm0 = 0,
    Xmm1 = 1,
    Xmm2 = 2,
    Xmm3 = 3,
    Xmm4 = 4,
    Xmm5 = 5,
    Xmm6 = 6,
    Xmm7 = 7,
};

enum class GeneralRegister
{
    Rax = 0,
    Rcx = 1,
    Rdx = 2,
    Rbx = 3,
    Rsp = 4,
    Rbp = 5,
    Rsi = 6,
    Rdi = 7,
};

enum class ExtendedGeneralRegister
{
    R8  = 0,
    R9  = 1,
    R10 = 2,
    R11 = 3,
    R12 = 4,
    R13 = 5,
    R14 = 6,
    R15 = 7,
};

constexpr ::std::size_t NumFloatRegisters { 8 };

constexpr ::std::size_t NumBytesInHomingSpace { 32 };

class MemoryOccupationManager
{
  private:
    ::std::bitset<NumFloatRegisters> floatRegisterOccupations_;

  public:
    auto GetOccupiedRegisters() noexcept
    {
        return ::std::views::iota(::std::size_t {}, NumFloatRegisters)
               | ::std::views::filter(
                   [this](::std::size_t idx) { return floatRegisterOccupations_.test(idx); })
               | ::std::views::transform(
                   [](::std::size_t idx) { return static_cast<FloatRegister>(idx); });
    }

    FloatRegister BorrowNewRegister()
    {
        if (floatRegisterOccupations_.all())
            throw ::std::runtime_error { "all registeres have been exhausted" };

        for (::std::size_t i {}; i < NumFloatRegisters; ++i)
        {
            if (!floatRegisterOccupations_.test(i))
            {
                floatRegisterOccupations_.set(i);
                return static_cast<FloatRegister>(i);
            }
        }

        [[unlikely]] throw ::std::runtime_error { "all registeres have been exhausted" };
    }

    void ReturnRegister(FloatRegister reg) noexcept
    {
        floatRegisterOccupations_.set(static_cast<::std::size_t>(reg), false);
    }

    bool BorrowRegister(FloatRegister reg) noexcept
    {
        if (!floatRegisterOccupations_.test(static_cast<::std::size_t>(reg)))
        {
            floatRegisterOccupations_.set(static_cast<::std::size_t>(reg));
            return true;
        }
        return false;
    }
};

template <typename T>
concept EightByteConstant = (sizeof(T) == sizeof(::std::uint64_t));

class CodeGenerator
{
  private:
    constexpr static ::std::size_t StackSizeIdx { 7 };

  private:
    constexpr static void OffsetMux(::std::ptrdiff_t offset, auto&& handler)
    {
        if (-::std::ptrdiff_t { 0x80 } <= offset && offset <= 0x7F)
        {
            ::std::uint8_t offsetCasted { static_cast<::std::uint8_t>(
                static_cast<::std::int8_t>(offset)) };
            handler(offsetCasted);
            return;
        }

        if (-::std::ptrdiff_t { 0x80000000 } <= offset && offset <= 0x7FFFFFFF)
        {
            ::std::uint32_t offsetCasted { static_cast<::std::uint8_t>(
                static_cast<::std::int32_t>(offset)) };
            handler(offsetCasted);
            return;
        }

        throw ::std::invalid_argument { "offset too large" };
    }

    constexpr void OffsetMux(auto                                    reg1,
                             auto                                    reg2,
                             ::std::ptrdiff_t                        reg2Offset,
                             ::std::initializer_list<::std::uint8_t> list,
                             ::std::initializer_list<::std::uint8_t> list2 = {})
    {
        OffsetMux(reg2Offset, [this, reg1, reg2, list, list2](auto offsetCasted) {
            if constexpr (::std::is_same_v<::std::uint8_t, ::std::decay_t<decltype(offsetCasted)>>)
            {
                ::std::uint8_t sourceDest {
                    static_cast<::std::uint8_t>(0x40 | (static_cast<int>(reg1) << 3)
                                                | static_cast<int>(reg2)),
                };

                EmitList(list);
                EmitList({ sourceDest });
                EmitList(list2);
                EmitList({ offsetCasted });
            }
            if constexpr (::std::is_same_v<::std::uint32_t, ::std::decay_t<decltype(offsetCasted)>>)
            {
                ::std::uint8_t sourceDest {
                    static_cast<::std::uint8_t>(0x80 | (static_cast<int>(reg1) << 3)
                                                | static_cast<int>(reg2)),
                };

                EmitList(list);
                EmitList({ sourceDest });
                EmitList(list2);
                EmitConstant(offsetCasted);
            }
        });
    }

  private:
    ::std::vector<::std::uint8_t> code_;

  public:
    constexpr ::std::vector<::std::uint8_t> const& code() const noexcept
    {
        return code_;
    }

  public:
    constexpr CodeGenerator() {}

  public:
    constexpr void EmitConstant(auto&& value)
    {
        ::std::array<::std::uint8_t, sizeof(value)> transmuted {
            Transmute<::std::decay_t<decltype(value)>, ::std::array<::std::uint8_t, sizeof(value)>>(
                value),
        };
        code_.insert(code_.end(), transmuted.begin(), transmuted.end());
    }

    constexpr void EmitList(::std::initializer_list<::std::uint8_t> list)
    {
        code_.insert(code_.end(), list);
    }

    constexpr void EmitProlog()
    {
        // clang-format off
        EmitList({
            // push rbp
            0x55,
            // mov rbp, rsp
            0x48, 0x89, 0xe5,
            // sub rsp, 0x00
            0x48, 0x83, 0xec, 0x00,
        });
        // clang-format on
    }

    constexpr void EmitEpilog(::std::size_t numBytesInStack)
    {
        // for 16-byte stack alignment requirement
        if (numBytesInStack % 16 > 0)
        {
            numBytesInStack += 16 - numBytesInStack % 16;
        }

        if (numBytesInStack <= 0x7F)
        {
            ::std::uint8_t operand { static_cast<::std::uint8_t>(numBytesInStack) };

            // modify 'sub rsp, 0x00' in the prolog
            code_[StackSizeIdx] = operand;
        }
        else if (numBytesInStack <= 0x7FFFFFFF)
        {
            code_[StackSizeIdx - 2] = 0x81;

            ::std::array<::std::uint8_t, 4> operand {
                Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                    static_cast<::std::uint32_t>(numBytesInStack)),
            };

            // modify 'sub rsp, 0x00' in the prolog
            code_[StackSizeIdx] = operand[0];
            code_.insert(code_.begin() + StackSizeIdx + 1, operand.begin() + 1, operand.end());
        }
        else
        {
            throw ::std::runtime_error { "stack size too big" };
        }

        EmitList({
            // leave
            0xc9,
            // ret
            0xc3,
        });
    }

    template <EightByteConstant T>
    constexpr void Mov(GeneralRegister dest, T constant)
    {
        EmitList({ 0x48, static_cast<::std::uint8_t>(0xb8 | static_cast<int>(dest)) });
        EmitConstant(constant);
    }

    template <EightByteConstant T>
    constexpr void Mov(ExtendedGeneralRegister dest, T constant)
    {
        EmitList({ 0x49, static_cast<::std::uint8_t>(0xb8 | static_cast<int>(dest)) });
        EmitConstant(constant);
    }

    constexpr void Mov(FloatRegister dest, FloatRegister src)
    {
        if (dest != src)
        {
            ::std::uint8_t instLastByte {
                static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                            | static_cast<int>(src)),
            };
            EmitList({ 0xf3, 0x0f, 0x7e, instLastByte });
        }
    }

    constexpr void Mov(GeneralRegister dest, ::std::ptrdiff_t offset, FloatRegister src)
    {
        if (dest != GeneralRegister::Rsp)
            OffsetMux(src, dest, offset, { 0x66, 0x0f, 0xd6 });
        else
            OffsetMux(src, dest, offset, { 0x66, 0x0f, 0xd6 }, { 0x24 });
    }

    constexpr void Mov(FloatRegister dest, GeneralRegister src, ::std::ptrdiff_t offset)
    {
        if (src != GeneralRegister::Rsp)
            OffsetMux(dest, src, offset, { 0xf3, 0x0f, 0x7e });
        else
            OffsetMux(dest, src, offset, { 0xf3, 0x0f, 0x7e }, { 0x24 });
    }

    constexpr void Mov(GeneralRegister dest, ::std::ptrdiff_t offset, GeneralRegister src)
    {
        OffsetMux(src, dest, offset, { 0x48, 0x89 });
    }

    constexpr void Mov(GeneralRegister dest, GeneralRegister src, ::std::ptrdiff_t offset)
    {
        OffsetMux(dest, src, offset, { 0x48, 0x8b });
    }

    constexpr void Mov(FloatRegister dest, GeneralRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                        | static_cast<int>(src)),
        };
        EmitList({ 0x66, 0x48, 0x0f, 0x6e, sourceDest });
    }

    constexpr void Mov(GeneralRegister dest, FloatRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(src) << 3)
                                        | static_cast<int>(dest)),
        };
        EmitList({ 0x66, 0x48, 0x0f, 0x7e, sourceDest });
    }

    constexpr void Addsd(FloatRegister dest, FloatRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                        | static_cast<int>(src)),
        };
        EmitList({ 0xf2, 0x0f, 0x58, sourceDest });
    }

    constexpr void Subsd(FloatRegister dest, FloatRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                        | static_cast<int>(src)),
        };
        EmitList({ 0xf2, 0x0f, 0x5c, sourceDest });
    }

    constexpr void Mulsd(FloatRegister dest, FloatRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                        | static_cast<int>(src)),
        };
        EmitList({ 0xf2, 0x0f, 0x59, sourceDest });
    }

    constexpr void Divsd(FloatRegister dest, FloatRegister src)
    {
        ::std::uint8_t sourceDest {
            static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                        | static_cast<int>(src)),
        };
        EmitList({ 0xf2, 0x0f, 0x5e, sourceDest });
    }

    constexpr void Call(GeneralRegister reg)
    {
        EmitList({ 0xff, static_cast<::std::uint8_t>(0xd0 | static_cast<int>(reg)) });
    }
};

}

/*

*/

::std::shared_ptr<CompiledFunction>
jitdemo::jit::Compile(::std::shared_ptr<ExpressionTreeFunction> const& function)
{
    ::std::vector<::std::shared_ptr<Function>> referencedFunctions;

    ::std::vector<::std::pair<Expression*, int>> expressionStack;
    ::std::vector<FloatRegister>                 evaluationStack;

    MemoryOccupationManager manager;
    CodeGenerator           generator;

    ::std::size_t numBytesInStack {};

    auto BackupOccupiedRegisters {
        [&generator, &manager, &numBytesInStack]() {
            ::std::size_t requiredNumStackBytes { 32 };
            ::std::size_t idx {};

            for (FloatRegister reg : manager.GetOccupiedRegisters())
            {
                // offset = numBytesInHomingSpace + idx * sizeof(double)
                ::std::size_t offset { NumBytesInHomingSpace + idx * sizeof(double) };

                generator.Mov(GeneralRegister::Rsp, offset, reg);

                requiredNumStackBytes += sizeof(double);
                ++idx;
            }
            numBytesInStack = ::std::max(numBytesInStack, requiredNumStackBytes);
        },
    };

    auto RestoreOccupiedRegisters {
        [&generator, &manager]() {
            ::std::size_t idx {};
            for (FloatRegister reg : manager.GetOccupiedRegisters())
            {
                // offset = numBytesInHomingSpace + idx * sizeof(double)
                ::std::size_t offset { NumBytesInHomingSpace + idx * sizeof(double) };

                generator.Mov(reg, GeneralRegister::Rsp, offset);

                ++idx;
            }
        },
    };

    generator.EmitProlog();

#ifdef _WIN32
    // rcx holds the value of the first argument in x64 Windows
    constexpr GeneralRegister paramReg { GeneralRegister::Rcx };
#else
    // rdi holds the value of the first argument in x64 Linux
    constexpr GeneralRegister paramReg { GeneralRegister::Rdi };
#endif

    generator.Mov(GeneralRegister::Rbp, -8, paramReg);

    expressionStack.push_back({ function->expr(), 0 });

    while (!expressionStack.empty())
    {
        auto [expression, numOperandsProcessed] { expressionStack.back() };
        if (auto constantExpression { dynamic_cast<ConstantExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            generator.Mov(GeneralRegister::Rax, constantExpression->value());
            generator.Mov(reg, GeneralRegister::Rax);

            evaluationStack.push_back(reg);
            expressionStack.pop_back();
        }
        else if (auto variableExpression { dynamic_cast<VariableExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            ::std::size_t offset { variableExpression->idx() * sizeof(double) };

            generator.Mov(reg, paramReg, offset);

            evaluationStack.push_back(reg);
            expressionStack.pop_back();
        }
        else if (auto binaryExpression { dynamic_cast<BinaryExpression*>(expression) })
        {
            switch (numOperandsProcessed)
            {
            case 0:
                expressionStack.back().second = 1;
                expressionStack.push_back({ binaryExpression->left(), 0 });
                continue;
            case 1:
                expressionStack.back().second = 2;
                expressionStack.push_back({ binaryExpression->right(), 0 });
                continue;
            case 2:
            {
                FloatRegister const rhs { evaluationStack.back() };
                evaluationStack.pop_back();

                FloatRegister const lhs { evaluationStack.back() };
                // evaluationStack.pop_back();

                manager.ReturnRegister(rhs);

                switch (binaryExpression->ops())
                {
                case BinaryExpressionOps::Addition: generator.Addsd(lhs, rhs); break;
                case BinaryExpressionOps::Subtraction: generator.Subsd(lhs, rhs); break;
                case BinaryExpressionOps::Multiplication: generator.Mulsd(lhs, rhs); break;
                case BinaryExpressionOps::Division: generator.Divsd(lhs, rhs); break;
                case BinaryExpressionOps::Power:

                    manager.ReturnRegister(lhs);
                    BackupOccupiedRegisters();

                    struct PowerFunction
                    {
                        static double Power(double lhs, double rhs) noexcept
                        {
                            return ::std::pow(lhs, rhs);
                        }
                    };

                    if (rhs == FloatRegister::Xmm0)
                    {
                        if (lhs == FloatRegister::Xmm1)
                        {
                            // swap
                            generator.Mov(FloatRegister::Xmm2, FloatRegister::Xmm0);
                            generator.Mov(FloatRegister::Xmm0, FloatRegister::Xmm1);
                            generator.Mov(FloatRegister::Xmm1, FloatRegister::Xmm2);
                        }
                        else
                        {
                            generator.Mov(FloatRegister::Xmm1, rhs);
                            generator.Mov(FloatRegister::Xmm0, lhs);
                        }
                    }
                    else
                    {
                        generator.Mov(FloatRegister::Xmm0, lhs);
                        generator.Mov(FloatRegister::Xmm1, rhs);
                    }

                    generator.Mov(GeneralRegister::Rax, &PowerFunction::Power);
                    generator.Call(GeneralRegister::Rax);
                    generator.Mov(lhs, FloatRegister::Xmm0);
                    generator.Mov(paramReg, GeneralRegister::Rbp, -8);

                    RestoreOccupiedRegisters();
                    manager.BorrowRegister(lhs);
                }

                // evaluationStack.push_back(lhs);
                expressionStack.pop_back();
            }
            }
        }
        else if (auto functionExpression { dynamic_cast<FunctionExpression*>(expression) })
        {
            if (numOperandsProcessed != 0)
            {
                FloatRegister const operandReg { evaluationStack.back() };
                evaluationStack.pop_back();

                // operandIdx = numOperandsProcessed - 1
                // offset = operandIdx * sizeof(double) + numBytesInHomingSpace
                // offset = numOperandsProcessed * sizeof(double) + 24
                ::std::size_t offset { numOperandsProcessed * sizeof(double) + 24 };

                // movq qword ptr [rsp + offset], xmm[operandReg]
            }

            if (numOperandsProcessed != functionExpression->GetNumArguments())
            {
                expressionStack.back().second = numOperandsProcessed + 1;
                expressionStack.push_back({
                    functionExpression->GetArgumentAt(numOperandsProcessed),
                    0,
                });
            }
            else
            {
                ::std::shared_ptr<Function> const& function { functionExpression->function() };
                referencedFunctions.push_back(function);

                struct FunctionCallProxy
                {
                    static double Call(Function*     func,
                                       double*       params,
                                       ::std::size_t numParams) noexcept
                    {
                        return func->Evaluate(::std::span<double> { params, numParams });
                    }
                };

                // Windows: rcx rdx r8
                // movabs r8, numParams
                // lea rdx, [rsp + numBytesInHomingSpace]
                // movabs rcx, function.get()
                // movabs rax, &FunctionCallProxy::Call
                // call rax
                //
                // Linux: rdi rsi rdx
                // movabs rdx, numParams
                // lea rsi, [rsp + numBytesInHomingSpace]
                // movabs rdi, function.get()
                // movabs rax, &FunctionCallProxy::Call
                // call rax
                // TODO
            }
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
        else
        {
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
    }

    generator.Mov(FloatRegister::Xmm0, evaluationStack.back());
    generator.EmitEpilog(numBytesInStack);

    return ::std::shared_ptr<CompiledFunction> {
        new CompiledFunction {
            function->params(),
            generator.code(),
            ::std::move(referencedFunctions),
        },
    };
}
