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
class Function : std::enable_shared_from_this<Function>
{
  private:
    std::unique_ptr<Expression> expr_;
    std::vector<std::u8string>  params_;

  public:
    Expression* expr() noexcept
    {
        return expr_.get();
    }

    std::vector<std::u8string> const& params() noexcept
    {
        return params_;
    }

  public:
    Function(std::vector<std::u8string>&& params, std::unique_ptr<Expression>&& expr) :
        params_ { std::move(params) },
        expr_ { std::move(expr) }
    {}

  public:
    virtual double Evaluate(std::span<double> params) noexcept
    {
        return expr_->Evaluate(params);
    }

    virtual ~Function() {}
};

}
