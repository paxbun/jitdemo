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
    std::unique_ptr<Expression> expr_;

  public:
    Expression* expr() noexcept
    {
        return expr_.get();
    }

  public:
    Function(std::unique_ptr<Expression>&& expr) : expr_ { std::move(expr) } {}

  public:
    virtual double Evaluate(std::span<double> params) noexcept
    {
        return expr_->Evaluate(params);
    }

    virtual ~Function() {}
};

}
