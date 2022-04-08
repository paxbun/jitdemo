// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <span>

export module jitdemo.expr.expression;

export namespace jitdemo::expr
{

/**
 * `Expression` represents a node in an expression tree.
 */
class Expression
{
  public:
    /**
     * Calculates the value of the expression for the given parameter values.
     * 
     * @param params contains the value of each function parameter.
     */
    virtual double Evaluate(std::span<double> params) noexcept = 0;

    virtual ~Expression() {}
};

}
