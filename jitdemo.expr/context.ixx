// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cmath>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

export module jitdemo.expr.context;

import jitdemo.expr.expression;
import jitdemo.expr.expressions;
import jitdemo.expr.function;

using jitdemo::expr::Expression;
using jitdemo::expr::Function;
using jitdemo::expr::expressions::BinaryBuiltinFunctionExpression;
using jitdemo::expr::expressions::UnaryBuiltinFunctionExpression;

export namespace jitdemo::expr
{

/**
 * `Context` contains a list of compiled functions in the current session.
 */
class Context final
{
  private:
    ::std::map<::std::u8string, ::std::shared_ptr<Function>> functions_;

  public:
    auto names() noexcept
    {
        return functions_
               | ::std::views::transform(
                   [](auto& pair) noexcept -> ::std::u8string_view { return pair.first; });
    }

  public:
    Context();

  public:
    /**
     * Returns the function with the given name.
     *
     * @param name the name of the function.
     * @return a pointer to the function
     */
    ::std::shared_ptr<Function> FindFunction(::std::u8string_view name) const noexcept
    {
        auto it { functions_.find(::std::u8string(name)) };
        if (it == functions_.end())
            return nullptr;

        return it->second;
    }

    /**
     * Adds a new function. If there was already a function with the same name, this function
     * has no effect.
     *
     * @param name the name of the function.
     * @param function a pointer to the function.
     * @return `true` if there was no function with the same name, `false` otherwise
     */
    bool AddFunction(::std::u8string_view               name,
                     ::std::shared_ptr<Function> const& function) noexcept
    {
        ::std::u8string nameAllocated { name };
        if (functions_.find(nameAllocated) != functions_.end())
            return false;

        functions_.insert(::std::make_pair(::std::move(nameAllocated), function));
        return true;
    }
};

}

module : private;

namespace
{

std::pair<::std::u8string, ::std::shared_ptr<Function>>
MakeUnaryFunction(const char8_t* name, UnaryBuiltinFunctionExpression::FunctionType function)
{
    return {
        name,
        ::std::shared_ptr<Function> {
            new Function {
                { u8"x" },
                ::std::unique_ptr<UnaryBuiltinFunctionExpression> {
                    new UnaryBuiltinFunctionExpression { function },
                },
            },
        },
    };
}

std::pair<::std::u8string, ::std::shared_ptr<Function>>
MakeBinaryFunction(const char8_t* name, BinaryBuiltinFunctionExpression::FunctionType function)
{
    return {
        name,
        ::std::shared_ptr<Function> {
            new Function {
                { u8"x", u8"y" },
                ::std::unique_ptr<BinaryBuiltinFunctionExpression> {
                    new BinaryBuiltinFunctionExpression { function },
                },
            },
        },
    };
}

}

jitdemo::expr::Context::Context() :
    functions_ {
        MakeUnaryFunction(u8"sin", [](double x) noexcept { return ::std::sin(x); }),
        MakeUnaryFunction(u8"cos", [](double x) noexcept { return ::std::cos(x); }),
        MakeUnaryFunction(u8"tan", [](double x) noexcept { return ::std::tan(x); }),
        MakeUnaryFunction(u8"asin", [](double x) noexcept { return ::std::asin(x); }),
        MakeUnaryFunction(u8"acos", [](double x) noexcept { return ::std::acos(x); }),
        MakeUnaryFunction(u8"atan", [](double x) noexcept { return ::std::atan(x); }),
        MakeUnaryFunction(u8"sinh", [](double x) noexcept { return ::std::sinh(x); }),
        MakeUnaryFunction(u8"cosh", [](double x) noexcept { return ::std::cosh(x); }),
        MakeUnaryFunction(u8"tanh", [](double x) noexcept { return ::std::tanh(x); }),
        MakeUnaryFunction(u8"ln", [](double x) noexcept { return ::std::log(x); }),
        MakeUnaryFunction(u8"log2", [](double x) noexcept { return ::std::log2(x); }),
        MakeUnaryFunction(u8"log10", [](double x) noexcept { return ::std::log10(x); }),
        MakeBinaryFunction(
            u8"log",
            [](double x, double y) noexcept { return ::std::log2(y) / ::std::log2(x); }),
    }
{}
