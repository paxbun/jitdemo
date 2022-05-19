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

    // sub rsp, 0x00
    code.insert(code.end(), { 0x48, 0x83, 0xec, 0x00 });

    expressionStack.push_back({ function->expr(), 0 });

    ::std::size_t numBytesInStack { 0 };

    while (!expressionStack.empty())
    {
        auto [expression, numOperandsProcessed] { expressionStack.back() };
        if (auto constantExpression { dynamic_cast<ConstantExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            ::std::array<::std::uint8_t, 8> value {
                Transmute<double, ::std::array<::std::uint8_t, 8>>(constantExpression->value()),
            };

            ::std::uint8_t sourceDestWithPrefix {
                static_cast<::std::uint8_t>(0xc0 | static_cast<int>(reg) << 3),
            };

            // movabs rax, value
            code.insert(code.end(), { 0x48, 0xB8 });
            code.insert(code.end(), value.begin(), value.end());

            // movq xmm[reg], rax
            code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x6e, sourceDestWithPrefix });

            evaluationStack.push_back(reg);
            expressionStack.pop_back();
        }
        else if (auto variableExpression { dynamic_cast<VariableExpression*>(expression) })
        {
            FloatRegister const reg { manager.BorrowNewRegister() };

            ::std::size_t offset { variableExpression->idx() * sizeof(double) };
#ifdef _WIN32
            // rcx holds the value of the first argument in x64 Windows
            constexpr int paramReg { 0x01 };
#else
            // rdi holds the value of the first argument in x64 Linux
            constexpr int paramReg { 0x07 };
#endif
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
                    static_cast<::std::uint8_t>(0xc0 | static_cast<int>(lhs) << 3
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

                    struct PowerFunction
                    {
                        static double Power(double lhs, double rhs) noexcept
                        {
                            return ::std::pow(lhs, rhs);
                        }
                    };

                    ::std::array<::std::uint8_t, 8> value {
                        Transmute<double (*)(double, double) noexcept,
                                  ::std::array<::std::uint8_t, 8>>(&PowerFunction::Power),
                    };

                    // sub rsp, 72 ; for parameter homing space
                    code.insert(code.end(), { 0x48, 0x83, 0xec, 0x48 });

                    // movavs rax, ::std::pow
                    code.insert(code.end(), { 0x48, 0xB8 });
                    code.insert(code.end(), value.begin(), value.end());
                    // call rax
                    code.insert(code.end(), { 0xff, 0xd0 });

                    // add rsp, 72
                    code.insert(code.end(), { 0x48, 0x83, 0xc4, 0x48 });

                    throw ::std::invalid_argument { "unexpected type of expression" };
                }

                // evaluationStack.push_back(lhs);
                expressionStack.pop_back();
            }
            }
        }
        else if (auto functionExpression { dynamic_cast<FunctionExpression*>(expression) })
        {
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
        else
        {
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
    }

    FloatRegister finalResultReg { evaluationStack.back() };
    if (finalResultReg != FloatRegister::Xmm0)
    {
        // mov xmm0, xmm[finalResultReg]
        ::std::uint8_t instLastByte {
            static_cast<::std::uint8_t>(0xc0 | static_cast<int>(finalResultReg)),
        };
        code.insert(code.end(), { 0xf3, 0x0f, 0x7e, instLastByte });
    }

    // for 16-byte stack alignment requirement
    if (numBytesInStack % 16 < 8)
    {
        numBytesInStack += 8 - numBytesInStack % 16;
    }
    else if (numBytesInStack % 16 > 8)
    {
        numBytesInStack += 24 - numBytesInStack % 16;
    }

    if (numBytesInStack <= 0x7F)
    {
        ::std::uint8_t operand { static_cast<::std::uint8_t>(numBytesInStack) };

        // modify 'sub rsp, 0x00' in the prolog
        code[3] = operand;

        // add rsp, numBytesInStack
        code.insert(code.end(), { 0x48, 0x83, 0xc4, operand });
    }
    else if (numBytesInStack <= 0x7FFFFFFF)
    {
        code[1] = 0x81;

        ::std::array<::std::uint8_t, 4> operand {
            Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                static_cast<::std::uint32_t>(numBytesInStack)),
        };

        // modify 'sub rsp, 0x00' in the prolog
        code[3] = operand[0];
        code.insert(code.begin() + 4, operand.begin() + 1, operand.end());

        // add rsp, numBytesInStack
        code.insert(code.end(), { 0x48, 0x81, 0xc4 });
        code.insert(code.end(), operand.begin(), operand.end());
    }
    else
    {
        throw ::std::runtime_error { "stack size too big" };
    }

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
