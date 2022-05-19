// Copyright (c) 2022 Chanjung Kim. All rights reserved.

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <span>
#include <string>
#include <vector>

import jitdemo.expr;
import jitdemo.expr.builtin_function;
import jitdemo.jit;

using ::jitdemo::expr::BinaryBuiltinFunction;
using ::jitdemo::expr::Context;
using ::jitdemo::expr::Function;
using ::jitdemo::expr::parsing::Parse;
using ::jitdemo::expr::parsing::Tokenize;
using ::jitdemo::jit::Compile;

int main(int argc, char** argv)
{
    Context context;

    ::std::string sourceNonUtf8 {
        "f(x, y) = (x + 1) * (y + 2) - (x + 3) ^ 5 / (x / 4) - 1.3 * y * y",
    };

    ::std::u8string_view source {
        reinterpret_cast<char8_t*>(sourceNonUtf8.data()),
        reinterpret_cast<char8_t*>(sourceNonUtf8.data() + sourceNonUtf8.size()),
    };

    auto result { Tokenize(source) };
    auto parseResult { Parse(context, result.tokens) };
    auto exprTreeFunction { parseResult.function };
    auto compiledFunction { Compile(exprTreeFunction) };
    auto nativeFunction {
        ::std::shared_ptr<Function> {
            new BinaryBuiltinFunction {
                [](double x, double y) noexcept {
                    return (x + 1) * (y + 2) - ::std::pow((x + 3), 5) / (x / 4) - 1.3 * y * y;
                },
            },
        },
    };

    ::std::vector<::std::pair<::std::string, ::std::shared_ptr<Function>>> functions {
        { "ExprTree", exprTreeFunction },
        { "Compiled", compiledFunction },
        { "Native", nativeFunction },
    };

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

    for (auto& [functionName, function] : functions)
    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        for (int i {}; i < 1'000'000; ++i)
        {
            double value { function->Evaluate(arguments) };
        }
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        double value { function->Evaluate(arguments) };
        ::std::cout << "Actual (" << functionName << "): " << std::setprecision(8) << value << '\n';
    }
    return 0;
}
