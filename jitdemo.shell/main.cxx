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

auto CheckTime(auto&& func)
{
    if constexpr (::std::is_same_v<void, decltype(func())>)
    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        func();
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";
    }
    else
    {
        ::std::chrono::high_resolution_clock clock {};

        auto start { clock.now() };
        auto result { func() };
        auto end { clock.now() };

        double seconds {
            ::std::chrono::duration_cast<::std::chrono::duration<double>>(end - start).count(),
        };
        ::std::cout << seconds << " s\n";

        return result;
    }
}

int main(int argc, char** argv)
{
    Context context;

    ::std::string sourceNonUtf8 {
        "f(x, y) = (x + 1) * (y + 2) - (x + 3) ^ 5 / (x / 4) - 1.3 * y * y",
    };
    ::std::cout << sourceNonUtf8 << "\n\n";

    ::std::u8string_view source {
        reinterpret_cast<char8_t*>(sourceNonUtf8.data()),
        reinterpret_cast<char8_t*>(sourceNonUtf8.data() + sourceNonUtf8.size()),
    };

    auto tokenStream { Tokenize(source) };

    ::std::cout << "Parsing: ";
    auto exprTreeFunction {
        CheckTime([&]() { return Parse(context, tokenStream.tokens).function; }),
    };
    ::std::cout << "JIT Compilation: ";
    auto compiledFunction {
        CheckTime([&]() { return Compile(exprTreeFunction); }),
    };
    auto nativeFunction {
        ::std::shared_ptr<Function> {
            new BinaryBuiltinFunction {
                [](double x, double y) noexcept {
                    return (x + 1) * (y + 2) - ::std::pow((x + 3), 5) / (x / 4) - 1.3 * y * y;
                },
            },
        },
    };
    ::std::cout << '\n';

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
        double value { function->Evaluate(arguments) };
        ::std::cout << "Actual (" << functionName << "): " << std::setprecision(8) << value << '\n';

        CheckTime([&]() {
            for (int i {}; i < 1'000'000; ++i)
            {
                double value { function->Evaluate(arguments) };
            }
        });
        ::std::cout << '\n';
    }
    return 0;
}
