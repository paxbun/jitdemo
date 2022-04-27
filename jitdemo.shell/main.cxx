// Copyright (c) 2022 Chanjung Kim. All rights reserved.

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <string>
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

    ::std::string sourceNonUtf8;
    ::std::getline(::std::cin, sourceNonUtf8);

    ::std::u8string_view source {
        reinterpret_cast<char8_t*>(sourceNonUtf8.data()),
        reinterpret_cast<char8_t*>(sourceNonUtf8.data() + sourceNonUtf8.size()),
    };

    auto result { Tokenize(source) };
    auto parseResult { Parse(context, result.tokens) };
    auto exprTreeFunction { parseResult.function };
    auto compiledFunction { Compile(exprTreeFunction) };

    ::std::random_device               randomDevice {};
    ::std::default_random_engine       engine { randomDevice() };
    ::std::normal_distribution<double> distribution {};

    ::std::vector<double> arguments;
    ::std::cout << "Arguments:\n";
    for (auto& param : exprTreeFunction->params())
    {
        double argument { distribution(engine) };
        ::std::cout << "  " << reinterpret_cast<const char*>(param.c_str());
        ::std::cout << " = " << argument << '\n';
        arguments.push_back(argument);
    }
    ::std::cout << '\n';

    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        for (int i {}; i < 1'000'000; ++i)
        {
            double value { exprTreeFunction->Evaluate(arguments) };
        }
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        double value { exprTreeFunction->Evaluate(arguments) };
        ::std::cout << "Actual (ExprTree): " << std::setprecision(8) << value << '\n';
    }

    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        for (int i {}; i < 1'000'000; ++i)
        {
            double value { compiledFunction->Evaluate(arguments) };
        }
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        double value { compiledFunction->Evaluate(arguments) };
        ::std::cout << "Actual (Compiled): " << std::setprecision(8) << value << '\n';
    }

    return 0;
}
