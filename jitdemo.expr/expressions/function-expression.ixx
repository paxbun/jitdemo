// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

export module jitdemo.expr.expressions.function_expression;

import jitdemo.expr.expression;
import jitdemo.expr.function;

using ::jitdemo::expr::Function;

export namespace jitdemo::expr::expressions
{

/**
 * `FunctionExpression` is an expression which contains a function call and its arguments.
 */
class FunctionExpression : public Expression
{
  private:
    ::std::shared_ptr<Function>                  function_;
    ::std::vector<::std::unique_ptr<Expression>> exprs_;

  public:
    ::std::shared_ptr<Function> const& function() const noexcept
    {
        return function_;
    }

  public:
    FunctionExpression(::std::shared_ptr<Function> const&             function,
                       ::std::vector<::std::unique_ptr<Expression>>&& expr) :
        function_ { function },
        exprs_ { ::std::move(expr) }
    {
        if (function->params().size() != exprs_.size())
            throw ::std::invalid_argument { "invalid number of arguments" };

        if (function_ == nullptr)
            throw ::std::invalid_argument { "function must not be nullptr" };
    }

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        ::std::vector<double> values;
        values.reserve(exprs_.size());

        for (auto& expr : exprs_)
        {
            values.push_back(expr->Evaluate(params));
        }
        return function_->Evaluate(values);
    }

    size_t GetNumArguments() const noexcept
    {
        return exprs_.size();
    }

    Expression* GetArgumentAt(::std::size_t idx) const
    {
        return exprs_[idx].get();
    }
};

}
