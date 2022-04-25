// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <span>
#include <string>
#include <vector>

export module jitdemo.expr.function;

import jitdemo.expr.expression;

export namespace jitdemo::expr
{

/**
 * `Function` represents a complete function which contains an expression.
 */
class Function
{
  private:
    ::std::vector<::std::u8string> params_;

  public:
    ::std::vector<::std::u8string> const& params() noexcept
    {
        return params_;
    }

  public:
    Function(::std::vector<::std::u8string>&& params) : params_ { ::std::move(params) } {}

  public:
    virtual double Evaluate(::std::span<double> params) noexcept = 0;

    virtual ~Function() {}
};

}
