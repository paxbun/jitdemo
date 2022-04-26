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
#include <cmath>
#include <memory>
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

}

::std::shared_ptr<CompiledFunction>
jitdemo::jit::Compile(::std::shared_ptr<ExpressionTreeFunction> const& function)
{
    ::std::vector<::std::shared_ptr<Function>>   referencedFunctions;
    ::std::vector<::std::uint8_t>                code;
    ::std::vector<::std::pair<Expression*, int>> expressionStack;
    expressionStack.push_back({ function->expr(), 0 });

    while (!expressionStack.empty())
    {
        auto [expression, numOperandsProcessed] { expressionStack.back() };
        if (auto binaryExpression { dynamic_cast<BinaryExpression*>(expression) }; binaryExpression)
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
                // pop rax
                code.push_back(0x58);
                // movq xmm1, rax
                code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x6e, 0xc8 });
                // pop rax
                code.push_back(0x58);
                // movq xmm0, rax
                code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x6e, 0xc0 });

                switch (binaryExpression->ops())
                {
                case BinaryExpressionOps::Addition:
                    // addsd xmm0, xmm1
                    code.insert(code.end(), { 0xf2, 0x0f, 0x58, 0xc1 });
                    break;
                case BinaryExpressionOps::Subtraction:
                    // subsd xmm0, xmm1
                    code.insert(code.end(), { 0xf2, 0x0f, 0x5c, 0xc1 });
                    break;
                case BinaryExpressionOps::Multiplication:
                    // mulsd xmm0, xmm1
                    code.insert(code.end(), { 0xf2, 0x0f, 0x59, 0xc1 });
                    break;
                case BinaryExpressionOps::Division:
                    // divsd xmm0, xmm1
                    code.insert(code.end(), { 0xf2, 0x0f, 0x5e, 0xc1 });
                    break;
                case BinaryExpressionOps::Power:
                {
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
                }
                }

                // movq rax, xmm0
                code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x7e, 0xc0 });
                // push rax
                code.push_back(0x50);

                expressionStack.pop_back();
            }
            }
        }
        else if (auto functionExpression { dynamic_cast<FunctionExpression*>(expression) };
                 functionExpression)
        {
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
        else if (auto constantExpression { dynamic_cast<ConstantExpression*>(expression) };
                 constantExpression)
        {
            ::std::array<::std::uint8_t, 8> value {
                Transmute<double, ::std::array<::std::uint8_t, 8>>(constantExpression->value()),
            };

            // movabs rax, value
            code.insert(code.end(), { 0x48, 0xB8 });
            code.insert(code.end(), value.begin(), value.end());

            // push rax
            code.push_back(0x50);

            expressionStack.pop_back();
        }
        else if (auto variableExpression { dynamic_cast<VariableExpression*>(expression) };
                 variableExpression)
        {
            ::std::size_t offset { variableExpression->idx() * sizeof(double) };
#ifdef _WIN32
            // mov rax, QWORD PTR [rcx + idx * sizeof(double)]
            if (offset <= 0xFF)
            {
                code.insert(code.end(), { 0x48, 0x8b, 0x41, static_cast<::std::uint8_t>(offset) });
            }
            else if (offset <= 0xFFFFFFFF)
            {
                ::std::array<::std::uint8_t, 4> value {
                    Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                        static_cast<::std::uint32_t>(offset)),
                };
                code.insert(code.end(), { 0x48, 0x8b, 0x81 });
                code.insert(code.end(), value.begin(), value.end());
            }
#else
            // mov rax, QWORD PTR [rdi + idx * sizeof(double)]
            if (offset <= 0xFF)
            {
                code.insert(code.end(), { 0x48, 0x8b, 0x47, static_cast<::std::uint8_t>(offset) });
            }
            else if (offset <= 0xFFFFFFFF)
            {
                ::std::array<::std::uint8_t, 4> value {
                    Transmute<::std::uint32_t, ::std::array<::std::uint8_t, 4>>(
                        static_cast<::std::uint32_t>(offset)),
                };
                code.insert(code.end(), { 0x48, 0x87, 0x81 });
                code.insert(code.end(), value.begin(), value.end());
            }
#endif
            // push rax
            code.push_back(0x50);

            expressionStack.pop_back();
        }
        else
        {
            throw ::std::invalid_argument { "unexpected type of expression" };
        }
    }

    // pop rax
    code.push_back(0x58);

    // movq xmm0, rax
    code.insert(code.end(), { 0x66, 0x48, 0x0f, 0x6e, 0xc0 });

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
