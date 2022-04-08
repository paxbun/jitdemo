// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cstring>

export module jitdemo.expr.expressions.variable_expression;

import jitdemo.expr.expression;

export namespace jitdemo::expr::expressions
{

/**
 * `VariableExpression` is an expression which has the value of a function parameter.
 */
class VariableExpression : public Expression
{
  private:
    std::size_t idx_;

  public:
    std::size_t idx() noexcept
    {
        return idx_;
    }

  public:
    VariableExpression(std::size_t idx) : idx_ { idx } {}

  public:
    virtual double Evaluate(std::span<double> params) noexcept override
    {
        if (params.size() <= idx_)
            return 0.0;

        return params[idx_];
    }
};

}
