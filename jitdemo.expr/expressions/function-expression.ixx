// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

export module jitdemo.expr.expressions.function_expression;

import jitdemo.expr.expression;
import jitdemo.expr.function;

using jitdemo::expr::Function;

export namespace jitdemo::expr::expressions
{

/**
 * `FunctionExpression` is an expression which contains a function call and its arguments.
 */
class FunctionExpression : public Expression
{
  private:
    Function*                                function_;
    std::vector<std::unique_ptr<Expression>> exprs_;

  public:
    FunctionExpression(Function* function, std::vector<std::unique_ptr<Expression>>&& expr) :
        function_ { function },
        exprs_ { std::move(expr) }
    {
        if (function_ == nullptr)
            throw std::invalid_argument { "function should not be nullptr" };
    }

  public:
    virtual double Evaluate(std::span<double> params) noexcept
    {
        std::vector<double> values;
        values.reserve(exprs_.size());

        for (auto& expr : exprs_)
        {
            values.push_back(expr->Evaluate(params));
        }
        return function_->Evaluate(values);
    }
};

}
