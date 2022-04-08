// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cmath>

export module jitdemo.expr.expressions.builtin_function_expression;

import jitdemo.expr.expression;

export namespace jitdemo::expr::expressions
{

class UnaryBuiltinFunctionExpression : Expression
{
  private:
    double (*func_)(double) noexcept;

  public:
    UnaryBuiltinFunctionExpression(double (*func)(double) noexcept) : func_ { func } {}

  public:
    virtual double Evaluate(std::span<double> params) noexcept override {
        if (params.size() < 1)
            return 0.0;

        return func_(params[0]);
    }
};

class BinaryBuiltinFunctionExpression : Expression
{
  private:
    double (*func_)(double, double) noexcept;

  public:
    virtual double Evaluate(std::span<double> params) noexcept override
    {
        if (params.size() < 1)
            return 0.0;

        else if (params.size() < 2)
            return func_(params[0], 0.0);

        return func_(params[0], params[1]);
    }
};

}
