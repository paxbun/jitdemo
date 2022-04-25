// Copyright (c) 2022 Chanjung Kim. All rights reserved.

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
    ::std::cout << parseResult.function->Evaluate(params);

    return 0;
}
