// Copyright (c) 2022 Chanjung Kim. All rights reserved.

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <span>
#include <vector>

import jitdemo.expr;
import jitdemo.jit;

using ::jitdemo::expr::Context;
using ::jitdemo::expr::parsing::Parse;
using ::jitdemo::expr::parsing::Tokenize;
using ::jitdemo::jit::Compile;

int main(int argc, char** argv)
{
    Context context;
    // auto    source { u8"f(x, y) = x^3 / x * sin(x) + .5 * log(x, 10.2) -  y   " };
    auto source { u8"f(x, y) = (x * 2 - 3 / y * y) * x" };
    auto result { Tokenize(source) };
    auto parseResult { Parse(context, result.tokens) };

    double params[] { 1123.3, 4.128973 };

    {
        double value {
            ::std::pow(params[0], 3) / params[0] * ::std::sin(params[0])
                + .5 * ::std::log(10.2) / ::std::log(params[0]) - params[1],
        };
        ::std::cout << "Expected: " << ::std::setprecision(8) << value << '\n';
    }

    auto exprTreeFunction { parseResult.function };
    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        for (int i {}; i < 1'000'000; ++i)
        {
            double value { exprTreeFunction->Evaluate(params) };
        }
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        double value { exprTreeFunction->Evaluate(params) };
        ::std::cout << "Actual (ExprTree): " << std::setprecision(8) << value << '\n';
    }

    auto compiledFunction { Compile(exprTreeFunction) };
    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        for (int i {}; i < 1'000'000; ++i)
        {
            double value { compiledFunction->Evaluate(params) };
        }
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        double value { compiledFunction->Evaluate(params) };
        ::std::cout << "Actual (Compiled): " << std::setprecision(8) << value << '\n';
    }

    return 0;
}
