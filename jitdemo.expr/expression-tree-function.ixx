// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <span>
#include <string>
#include <vector>

export module jitdemo.expr.expression_tree_function;

import jitdemo.expr.function;
import jitdemo.expr.expression;

export namespace jitdemo::expr
{

class ExpressionTreeFunction final : public Function
{
  private:
    ::std::unique_ptr<Expression> expr_;

  public:
    Expression* expr() noexcept
    {
        return expr_.get();
    }

  public:
    ExpressionTreeFunction(::std::vector<::std::u8string>&& params,
                           ::std::unique_ptr<Expression>&&  expr) :
        Function { ::std::move(params) },
        expr_ { ::std::move(expr) }
    {}

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        return expr_->Evaluate(params);
    }
};

}
