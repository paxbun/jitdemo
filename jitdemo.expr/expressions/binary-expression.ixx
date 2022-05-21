// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cmath>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

export module jitdemo.expr.expressions.binary_expression;

import jitdemo.expr.expression;

export namespace jitdemo::expr::expressions
{

enum class BinaryExpressionOps
{
    Addition,
    Subtraction,
    Multiplication,
    Division,
    Power,
};

/**
 * `BinaryExpression` is an expression with a binary operator.
 */
class BinaryExpression : public Expression
{
  private:
    ::std::unique_ptr<Expression> left_;
    ::std::unique_ptr<Expression> right_;
    BinaryExpressionOps           ops_;

  public:
    Expression* left() noexcept
    {
        return left_.get();
    }

    Expression* right() noexcept
    {
        return right_.get();
    }

    BinaryExpressionOps ops() noexcept
    {
        return ops_;
    }

  public:
    BinaryExpression(::std::unique_ptr<Expression>&& left,
                     ::std::unique_ptr<Expression>&& right,
                     BinaryExpressionOps             ops) :
        left_ { ::std::move(left) },
        right_ { ::std::move(right) },
        ops_ { ops }
    {
        if (ops_ < BinaryExpressionOps::Addition || BinaryExpressionOps::Power < ops_)
            throw ::std::invalid_argument { "invalid operation" };
    }

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        double leftValue { left_->Evaluate(params) };
        double rightValue { right_->Evaluate(params) };
        switch (ops_)
        {
        case BinaryExpressionOps::Addition: return leftValue + rightValue;
        case BinaryExpressionOps::Subtraction: return leftValue - rightValue;
        case BinaryExpressionOps::Multiplication: return leftValue * rightValue;
        case BinaryExpressionOps::Division: return leftValue / rightValue;
        case BinaryExpressionOps::Power: return ::std::pow(leftValue, rightValue);
        default: [[unlikely]] return 0.0;
        }
    }
};

}
