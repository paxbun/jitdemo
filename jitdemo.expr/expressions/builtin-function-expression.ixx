// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cmath>
#include <stdexcept>

export module jitdemo.expr.expressions.builtin_function_expression;

import jitdemo.expr.expression;

export namespace jitdemo::expr::expressions
{

class UnaryBuiltinFunctionExpression : public Expression
{
  public:
    using FunctionType = double (*)(double) noexcept;

  private:
    FunctionType func_;

  public:
    FunctionType func() noexcept
    {
        return func_;
    }

  public:
    UnaryBuiltinFunctionExpression(FunctionType func) : func_ { func }
    {
        if (func == nullptr)
            throw ::std::invalid_argument { "func must not be nullptr" };
    }

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        if (params.size() < 1)
            return 0.0;

        return func_(params[0]);
    }
};

class BinaryBuiltinFunctionExpression : public Expression
{
  public:
    using FunctionType = double (*)(double, double) noexcept;

  private:
    FunctionType func_;

  public:
    FunctionType func() noexcept
    {
        return func_;
    }

  public:
    BinaryBuiltinFunctionExpression(FunctionType func) : func_ { func }
    {
        if (func == nullptr)
            throw ::std::invalid_argument { "func must not be nullptr" };
    }

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        if (params.size() < 1)
            return 0.0;

        else if (params.size() < 2)
            return func_(params[0], 0.0);

        return func_(params[0], params[1]);
    }
};

}
