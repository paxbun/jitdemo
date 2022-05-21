// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <span>

export module jitdemo.expr.expressions.constant_expression;

import jitdemo.expr.expression;

export namespace jitdemo::expr::expressions
{

/**
 * `ConstantExpression` is an expression with an immutable value.
 */
class ConstantExpression : public Expression
{
  private:
    double value_;

  public:
    double value() noexcept
    {
        return value_;
    }

  public:
    ConstantExpression(double value) : value_ { value } {}

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        return value_;
    }
};

}
