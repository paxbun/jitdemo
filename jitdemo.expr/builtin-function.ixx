// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cmath>
#include <span>
#include <stdexcept>

export module jitdemo.expr.builtin_function;

import jitdemo.expr.function;

export namespace jitdemo::expr
{

class UnaryBuiltinFunction final : public Function
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
    UnaryBuiltinFunction(FunctionType func) : Function { { u8"x" } }, func_ { func }
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

class BinaryBuiltinFunction final : public Function
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
    BinaryBuiltinFunction(FunctionType func) : Function { { u8"x", u8"y" } }, func_ { func }
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
