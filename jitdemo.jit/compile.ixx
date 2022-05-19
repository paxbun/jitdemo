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
};

}

/*

*/

::std::shared_ptr<CompiledFunction>
jitdemo::jit::Compile(::std::shared_ptr<ExpressionTreeFunction> const& function)
{
    ::std::vector<::std::shared_ptr<Function>> referencedFunctions;
    ::std::vector<::std::uint8_t>              code;

    ::std::vector<::std::pair<Expression*, int>> expressionStack;
    ::std::vector<FloatRegister>                 evaluationStack;

    MemoryOccupationManager manager;

    auto InsertMovAbs {
        [&code](auto&& value) {
            ::std::array<::std::uint8_t, 8> transmuted {
                Transmute<::std::decay_t<decltype(value)>, ::std::array<::std::uint8_t, 8>>(value),
            };
            code.insert(code.end(), { 0x48, 0xB8 });
            code.insert(code.end(), transmuted.begin(), transmuted.end());
        },
    };

    auto Mov {
        [&code](FloatRegister dest, FloatRegister src) {
            if (dest != src)
            {
                // mov xmm[dest], xmm[src]
                ::std::uint8_t instLastByte {
                    static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(dest) << 3)
                                                | static_cast<int>(src)),
                };
                code.insert(code.end(), { 0xf3, 0x0f, 0x7e, instLastByte });
            }
        },
    };

    // push rbp
    code.insert(code.end(), { 0x55 });
    // mov rbp, rsp
    code.insert(code.end(), { 0x48, 0x89, 0xe5 });
    // sub rsp, 0x00
    code.insert(code.end(), { 0x48, 0x83, 0xec, 0x00 });
    constexpr ::std::size_t stackSizeIdx { 7 };

#ifdef _WIN32
    // rcx holds the value of the first argument in x64 Windows
    constexpr int paramReg { 0x01 };
#else
    // rdi holds the value of the first argument in x64 Linux
    constexpr int paramReg { 0x07 };
#endif

    // mov qword ptr [rbp - 0x08], paramReg
    code.insert(code.end(), { 0x48, 0x89, 0x45 | (paramReg << 3), 0xf8 });

    expressionStack.push_back({ function->expr(), 0 });

    ::std::size_t numBytesInStack { 0 };

    while (!expressionStack.empty())
    {
        auto [expression, numOperandsProcessed] { expressionStack.back() };
        if (auto constantExpression { dynamic_cast<ConstantExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            ::std::uint8_t sourceDestWithPrefix {
                static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(reg) << 3)),
            };

            // movabs rax, value
            InsertMovAbs(constantExpression->value());

            // movq xmm[reg], rax
            code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x6e, sourceDestWithPrefix });

            evaluationStack.push_back(reg);
            expressionStack.pop_back();
        }
        else if (auto variableExpression { dynamic_cast<VariableExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            ::std::size_t offset { variableExpression->idx() * sizeof(double) };

            // movq xmm[reg], QWORD PTR [(rcx if windows else rdi) + idx * sizeof(double)]
            if (offset <= 0x7F)
            {
                code.insert(
                    code.end(),
                    {
                        0xf3,
                        0x0f,
                        0x7e,
                        static_cast<::std::uint8_t>(0x40 | paramReg | (static_cast<int>(reg) << 3)),
                        static_cast<::std::uint8_t>(offset),
                    });
            }
            else if (offset <= 0xFFFFFFFF)
            {
                ::std::array<::std::uint8_t, 4> value {
                    Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                        static_cast<::std::uint32_t>(offset)),
                };
                code.insert(
                    code.end(),
                    {
                        0xf3,
                        0x0f,
                        0x7e,
                        static_cast<::std::uint8_t>(0x80 | paramReg | (static_cast<int>(reg) << 3)),
                    });
                code.insert(code.end(), value.begin(), value.end());
            }
            else
            {
                throw ::std::runtime_error { "offset too big" };
            }

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

                ::std::uint8_t sourceDestWithPrefix {
                    static_cast<::std::uint8_t>(0xc0 | (static_cast<int>(lhs) << 3)
                                                | static_cast<int>(rhs)),
                };

                manager.ReturnRegister(rhs);

                switch (binaryExpression->ops())
                {
                case BinaryExpressionOps::Addition:
                    // addsd xmm[lhs], xmm[rhs]
                    code.insert(code.end(), { 0xf2, 0x0f, 0x58, sourceDestWithPrefix });
                    break;
                case BinaryExpressionOps::Subtraction:
                    // subsd xmm[lhs], xmm[rhs]
                    code.insert(code.end(), { 0xf2, 0x0f, 0x5c, sourceDestWithPrefix });
                    break;
                case BinaryExpressionOps::Multiplication:
                    // mulsd xmm[lhs], xmm[rhs]
                    code.insert(code.end(), { 0xf2, 0x0f, 0x59, sourceDestWithPrefix });
                    break;
                case BinaryExpressionOps::Division:
                    // divsd xmm[lhs], xmm[rhs]
                    code.insert(code.end(), { 0xf2, 0x0f, 0x5e, sourceDestWithPrefix });
                    break;
                case BinaryExpressionOps::Power:
                {
                    {
                        ::std::size_t requiredNumStackBytes { 32 };
                        ::std::size_t idx {};
                        for (FloatRegister reg : manager.GetOccupiedRegisters())
                        {
                            if (reg == lhs)
                                continue;

                            ::std::uint8_t sourceWithPrefix {
                                static_cast<::std::uint8_t>(0x44 | (static_cast<int>(reg) << 3)),
                            };

                            // offset = numBytesInHomingSpace + idx * sizeof(double)
                            // Since the number of registers is up to 8, offset is always less than
                            // 128
                            auto offset {
                                static_cast<::std::uint8_t>(NumBytesInHomingSpace
                                                            + idx * sizeof(double)),
                            };

                            // movq qword ptr [rsp + offset], xmm[reg]
                            code.insert(code.end(),
                                        { 0x66, 0x0f, 0xd6, sourceWithPrefix, 0x24, offset });

                            requiredNumStackBytes += sizeof(double);
                            ++idx;
                        }
                        numBytesInStack = ::std::max(numBytesInStack, requiredNumStackBytes);
                    }

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
                            Mov(FloatRegister::Xmm2, FloatRegister::Xmm0);
                            Mov(FloatRegister::Xmm0, FloatRegister::Xmm1);
                            Mov(FloatRegister::Xmm1, FloatRegister::Xmm2);
                        }
                        else
                        {
                            Mov(FloatRegister::Xmm1, rhs);
                            Mov(FloatRegister::Xmm0, lhs);
                        }
                    }
                    else
                    {
                        Mov(FloatRegister::Xmm0, lhs);
                        Mov(FloatRegister::Xmm1, rhs);
                    }

                    // movabs rax, ::std::pow
                    InsertMovAbs(&PowerFunction::Power);

                    // call rax
                    code.insert(code.end(), { 0xff, 0xd0 });

                    // mov xmm[lhs], xmm0
                    Mov(lhs, FloatRegister::Xmm0);

                    // mov paramReg, qword ptr [rbp - 0x08]
                    code.insert(code.end(), { 0x48, 0x8b, 0x45 | (paramReg << 3), 0xf8 });

                    {
                        ::std::size_t idx {};
                        for (FloatRegister reg : manager.GetOccupiedRegisters())
                        {
                            if (reg == lhs)
                                continue;

                            ::std::uint8_t sourceWithPrefix {
                                static_cast<::std::uint8_t>(0x44 | (static_cast<int>(reg) << 3)),
                            };

                            // offset = numBytesInHomingSpace + idx * sizeof(double)
                            // Since the number of registers is up to 8, offset is always less than
                            // 128
                            auto offset {
                                static_cast<::std::uint8_t>(NumBytesInHomingSpace
                                                            + idx * sizeof(double)),
                            };

                            // movq xmm[reg], qword ptr [rsp + offset]
                            code.insert(code.end(),
                                        { 0xf3, 0x0f, 0x7e, sourceWithPrefix, 0x24, offset });

                            ++idx;
                        }
                    }
                }
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

                // mov qword ptr [rsp + offset], xmm[operandReg]
                // TODO
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

    // mov xmm0, xmm[finalResultReg]
    Mov(FloatRegister::Xmm0, evaluationStack.back());

    // for 16-byte stack alignment requirement
    if (numBytesInStack % 16 > 0)
    {
        numBytesInStack += 16 - numBytesInStack % 16;
    }

    if (numBytesInStack <= 0x7F)
    {
        ::std::uint8_t operand { static_cast<::std::uint8_t>(numBytesInStack) };

        // modify 'sub rsp, 0x00' in the prolog
        code[stackSizeIdx] = operand;
    }
    else if (numBytesInStack <= 0x7FFFFFFF)
    {
        code[stackSizeIdx - 2] = 0x81;

        ::std::array<::std::uint8_t, 4> operand {
            Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                static_cast<::std::uint32_t>(numBytesInStack)),
        };

        // modify 'sub rsp, 0x00' in the prolog
        code[stackSizeIdx] = operand[0];
        code.insert(code.begin() + stackSizeIdx + 1, operand.begin() + 1, operand.end());
    }
    else
    {
        throw ::std::runtime_error { "stack size too big" };
    }

    // leave
    code.push_back(0xc9);
    // ret
    code.push_back(0xc3);

    return ::std::shared_ptr<CompiledFunction> {
        new CompiledFunction {
            function->params(),
            code,
            ::std::move(referencedFunctions),
        },
    };
}
