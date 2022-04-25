// Copyright (c) 2022 Chanjung Kim. All rights reserved.

#include <cmath>
#include <iomanip>
#include <iostream>
#include <span>
#include <vector>

import jitdemo.expr;

using ::jitdemo::expr::Context;
using ::jitdemo::expr::parsing::Parse;
using ::jitdemo::expr::parsing::Tokenize;

int main(int argc, char** argv)
{
    Context context;
    auto    source { u8"f(x, y) = x^3 / x * sin(x) + .5 * log(x, 10.2) -  y   " };
    auto    result { Tokenize(source) };
    auto    parseResult { Parse(context, result.tokens) };

    double params[] { 1.3, 4.6 };
    double expectedValue {
        ::std::pow(params[0], 3) / params[0] * ::std::sin(params[0])
            + .5 * ::std::log(10.2) / ::std::log(params[0]) - params[1],
    };
    double value { parseResult.function->Evaluate(params) };
    ::std::cout << "Expected: " << ::std::setprecision(8) << expectedValue << '\n';
    ::std::cout << "Actual: " << std::setprecision(8) << value << '\n';
    ::std::cout << "Ok: " << ::std::boolalpha << (::std::abs(value - 1.45429756154) < 1e-8) << '\n';

    return 0;
}
