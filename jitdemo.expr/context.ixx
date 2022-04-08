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
class Context
{
  private:
    std::map<std::string, std::shared_ptr<Function>> functions_;

  public:
    auto names() noexcept
    {
        return functions_ | std::views::transform([](auto& pair) noexcept -> std::string_view {
                   return pair.first;
               });
    }

  public:
    Context();

  public:
    std::shared_ptr<Function> FindFunction(std::string_view name) noexcept
    {
        auto it { functions_.find(std::string(name)) };
        if (it == functions_.end())
            return nullptr;

        return it->second;
    }

    bool AddFunction(std::string_view name, std::shared_ptr<Function> const& function) noexcept
    {
        std::string nameAllocated { name };
        if (functions_.find(nameAllocated) != functions_.end())
            return false;

        functions_.insert(std::make_pair(std::move(nameAllocated), function));
        return true;
    }
};

}

module : private;

namespace
{

std::pair<std::string, std::shared_ptr<Function>>
MakeUnaryFunction(const char* name, UnaryBuiltinFunctionExpression::FunctionType function)
{
    return {
        name,
        std::shared_ptr<Function> {
            new Function {
                { "x" },
                std::unique_ptr<UnaryBuiltinFunctionExpression> {
                    new UnaryBuiltinFunctionExpression { function },
                },
            },
        },
    };
}

std::pair<std::string, std::shared_ptr<Function>>
MakeBinaryFunction(const char* name, BinaryBuiltinFunctionExpression::FunctionType function)
{
    return {
        name,
        std::shared_ptr<Function> {
            new Function {
                { "x", "y" },
                std::unique_ptr<BinaryBuiltinFunctionExpression> {
                    new BinaryBuiltinFunctionExpression { function },
                },
            },
        },
    };
}

}

jitdemo::expr::Context::Context() :
    functions_ {
        MakeUnaryFunction("sin", [](double x) noexcept { return std::sin(x); }),
        MakeUnaryFunction("cos", [](double x) noexcept { return std::cos(x); }),
        MakeUnaryFunction("tan", [](double x) noexcept { return std::tan(x); }),
        MakeUnaryFunction("asin", [](double x) noexcept { return std::asin(x); }),
        MakeUnaryFunction("acos", [](double x) noexcept { return std::acos(x); }),
        MakeUnaryFunction("atan", [](double x) noexcept { return std::atan(x); }),
        MakeUnaryFunction("sinh", [](double x) noexcept { return std::sinh(x); }),
        MakeUnaryFunction("cosh", [](double x) noexcept { return std::cosh(x); }),
        MakeUnaryFunction("tanh", [](double x) noexcept { return std::tanh(x); }),
        MakeUnaryFunction("ln", [](double x) noexcept { return std::log(x); }),
        MakeUnaryFunction("log2", [](double x) noexcept { return std::log2(x); }),
        MakeUnaryFunction("log10", [](double x) noexcept { return std::log10(x); }),
        MakeBinaryFunction("log",
                           [](double x, double y) noexcept { return std::log2(y) / std::log2(x); }),
    }
{}
