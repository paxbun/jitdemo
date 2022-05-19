// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <charconv>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

export module jitdemo.expr.parsing.parse;

import jitdemo.expr.compilation_error;
import jitdemo.expr.context;
import jitdemo.expr.expression;
import jitdemo.expr.expression_tree_function;
import jitdemo.expr.function;
import jitdemo.expr.expressions.binary_expression;
import jitdemo.expr.expressions.constant_expression;
import jitdemo.expr.expressions.variable_expression;
import jitdemo.expr.expressions.function_expression;
import jitdemo.expr.parsing.token;

export namespace jitdemo::expr::parsing
{

struct ParsingResult
{
    ::std::u8string_view                      functionName;
    ::std::shared_ptr<ExpressionTreeFunction> function;
    ::std::vector<CompilationError>           errors;
};

ParsingResult Parse(Context const& context, ::std::span<Token> tokens) noexcept;

}

module : private;

using ::jitdemo::expr::CompilationError;
using ::jitdemo::expr::CompilationErrorType;
using ::jitdemo::expr::Context;
using ::jitdemo::expr::Expression;
using ::jitdemo::expr::ExpressionTreeFunction;
using ::jitdemo::expr::Function;
using ::jitdemo::expr::expressions::BinaryExpression;
using ::jitdemo::expr::expressions::BinaryExpressionOps;
using ::jitdemo::expr::expressions::ConstantExpression;
using ::jitdemo::expr::expressions::FunctionExpression;
using ::jitdemo::expr::expressions::VariableExpression;
using ::jitdemo::expr::parsing::ParsingResult;
using ::jitdemo::expr::parsing::Token;
using ::jitdemo::expr::parsing::TokenType;

template <typename T>
using Shared = ::std::shared_ptr<T>;

template <typename T>
using Unique = ::std::unique_ptr<T>;

// Syntax Definition
//
// Func        ::   FuncSig "=" Expr
// FuncSig     ::   Identifier "(" ( Identifier ( "," Identifier )* )? ")"
// Expr        ::   AddExpr
// AddExpr     ::   MulExpr ( ("+" | "-") MulExpr )*
// MulExpr     ::   PowExpr ( ("*" | "/") PowExpr )*
// PowExpr     ::   ParenExpr ( "^" | ParenExpr )*
// ParenExpr   ::   "(" Expr ")"
//             ::   IdenExpr
// IdenExpr    ::   Identifier "(" ( Expr ( "," Expr )* )? ")"
//             ::   Identifier
//             ::   Numeric

namespace
{

template <typename L, typename... Rs>
bool IsOneOf(L&& l, Rs&&... rs)
{
    return ((l == rs) || ...);
}

class ParsingState
{
  private:
    ::std::span<Token>::iterator current;
    ::std::span<Token>::iterator end;

  public:
    ParsingState(::std::span<Token> span) noexcept : current { span.begin() }, end { span.end() } {}

  public:
    Token const& Next() const
    {
        // All token streams must end with TokenType::EndOfFile
        if (current == end)
        {
            throw CompilationError {
                .type { CompilationErrorType::UnknownError },
                .begin { 0 },
                .end { 0 },
            };
        }

        return *current;
    }

    template <typename... Ts>
    bool NextHasType(Ts&&... types) const
    {
        return IsOneOf(Next().type, ::std::forward<Ts>(types)...);
    }

    template <typename... Ts>
    Token const& Match(Ts&&... types)
    {
        Token const& next = Next();
        if (IsOneOf(next.type, ::std::forward<Ts>(types)...))
        {
            ++current;
            return next;
        }
        else
        {
            throw CompilationError {
                .type { CompilationErrorType::UnexpectedToken },
                .begin { next.begin },
                .end { next.end },
            };
        }
    }
};

struct FunctionSignature
{
    ::std::u8string_view                functionName;
    ::std::vector<::std::u8string_view> parameterNames;
};

struct FunctionWithName
{
    ::std::u8string_view           functionName;
    Shared<ExpressionTreeFunction> function;
};

FunctionWithName ParseFunc(ParsingState& state, Context const& context);

FunctionSignature ParseFuncSig(ParsingState& state);

#define EXPR_PARSER_PARAMS                                                                         \
    ParsingState &state, Context const &context, FunctionSignature const &signature

#define FORWARD_EXPR_PARSER_PARAMS state, context, signature

Unique<Expression> ParseExpr(EXPR_PARSER_PARAMS);

Unique<Expression> ParseAddExpr(EXPR_PARSER_PARAMS);

Unique<Expression> ParseMulExpr(EXPR_PARSER_PARAMS);

Unique<Expression> ParsePowExpr(EXPR_PARSER_PARAMS);

Unique<Expression> ParseParenExpr(EXPR_PARSER_PARAMS);

Unique<Expression> ParseIdenExpr(EXPR_PARSER_PARAMS);

FunctionWithName ParseFunc(ParsingState& state, Context const& context)
{
    FunctionSignature signature { ParseFuncSig(state) };
    state.Match(TokenType::Equals);
    Unique<Expression> expression { ParseExpr(FORWARD_EXPR_PARSER_PARAMS) };
    state.Match(TokenType::EndOfFile);

    return FunctionWithName {
        .functionName { signature.functionName },
        .function {
            Shared<ExpressionTreeFunction> {
                new ExpressionTreeFunction {
                    ::std::vector<::std::u8string>(signature.parameterNames.begin(),
                                                   signature.parameterNames.end()),
                    ::std::move(expression),
                },
            },
        },
    };
}

FunctionSignature ParseFuncSig(ParsingState& state)
{
    ::std::u8string_view                functionName { state.Match(TokenType::Identifier).text };
    ::std::vector<::std::u8string_view> parameterNames;

    state.Match(TokenType::ParenthesisOpen);
    if (state.NextHasType(TokenType::Identifier))
    {
        parameterNames.push_back(state.Match(TokenType::Identifier).text);
        while (state.NextHasType(TokenType::Comma))
        {
            state.Match(TokenType::Comma);
            parameterNames.push_back(state.Match(TokenType::Identifier).text);
        }
    }
    state.Match(TokenType::ParenthesisClose);

    return FunctionSignature {
        .functionName { ::std::move(functionName) },
        .parameterNames { ::std::move(parameterNames) },
    };
}

Unique<Expression> ParseExpr(EXPR_PARSER_PARAMS)
{
    return ParseAddExpr(FORWARD_EXPR_PARSER_PARAMS);
}

Unique<Expression> ParseAddExpr(EXPR_PARSER_PARAMS)
{
    Unique<Expression> left { ParseMulExpr(FORWARD_EXPR_PARSER_PARAMS) };
    while (state.NextHasType(TokenType::Plus, TokenType::Minus))
    {
        TokenType operatorTokenType {
            state.Match(TokenType::Plus, TokenType::Minus).type,
        };
        Unique<Expression> right { ParseMulExpr(FORWARD_EXPR_PARSER_PARAMS) };
        left = Unique<BinaryExpression> {
            new BinaryExpression {
                ::std::move(left),
                ::std::move(right),
                operatorTokenType == TokenType::Plus ? BinaryExpressionOps::Addition
                                                     : BinaryExpressionOps::Subtraction,
            },
        };
    }

    return left;
}

Unique<Expression> ParseMulExpr(EXPR_PARSER_PARAMS)
{
    Unique<Expression> left { ParsePowExpr(FORWARD_EXPR_PARSER_PARAMS) };
    while (state.NextHasType(TokenType::Asterisk, TokenType::Slash))
    {
        TokenType operatorTokenType {
            state.Match(TokenType::Asterisk, TokenType::Slash).type,
        };
        Unique<Expression> right { ParsePowExpr(FORWARD_EXPR_PARSER_PARAMS) };
        left = Unique<Expression> {
            new BinaryExpression {
                ::std::move(left),
                ::std::move(right),
                operatorTokenType == TokenType::Asterisk ? BinaryExpressionOps::Multiplication
                                                         : BinaryExpressionOps::Division,
            },
        };
    }

    return left;
}

Unique<Expression> ParsePowExpr(EXPR_PARSER_PARAMS)
{
    Unique<Expression> left { ParseParenExpr(FORWARD_EXPR_PARSER_PARAMS) };
    while (state.NextHasType(TokenType::Caret))
    {
        state.Match(TokenType::Caret);
        Unique<Expression> right { ParseParenExpr(FORWARD_EXPR_PARSER_PARAMS) };
        left = Unique<BinaryExpression> {
            new BinaryExpression {
                ::std::move(left),
                ::std::move(right),
                BinaryExpressionOps::Power,
            },
        };
    }

    return left;
}

Unique<Expression> ParseParenExpr(EXPR_PARSER_PARAMS)
{
    if (state.NextHasType(TokenType::ParenthesisOpen))
    {
        state.Match(TokenType::ParenthesisOpen);
        Unique<Expression> rtn { ParseExpr(FORWARD_EXPR_PARSER_PARAMS) };
        state.Match(TokenType::ParenthesisClose);

        return rtn;
    }

    return ParseIdenExpr(FORWARD_EXPR_PARSER_PARAMS);
}

Unique<Expression> ParseIdenExpr(EXPR_PARSER_PARAMS)
{
    if (state.NextHasType(TokenType::Identifier))
    {
        Token const&         identifierToken { state.Match(TokenType::Identifier) };
        ::std::u8string_view identifier { identifierToken.text };

        // if the expression is a function call
        if (state.NextHasType(TokenType::ParenthesisOpen))
        {
            Shared<Function> function { context.FindFunction(identifier) };
            if (!function)
            {
                throw CompilationError {
                    .type { CompilationErrorType::UndefinedFunction },
                    .begin { identifierToken.begin },
                    .end { identifierToken.end },
                };
            }

            ::std::vector<Unique<Expression>> arguments;
            state.Match(TokenType::ParenthesisOpen);

            if (!state.NextHasType(TokenType::ParenthesisClose))
            {
                arguments.push_back(ParseExpr(FORWARD_EXPR_PARSER_PARAMS));
                while (state.NextHasType(TokenType::Comma))
                {
                    state.Match(TokenType::Comma);
                    arguments.push_back(ParseExpr(FORWARD_EXPR_PARSER_PARAMS));
                }
            }

            Token const& parenthesisCloseToken { state.Match(TokenType::ParenthesisClose) };

            if (function->params().size() != arguments.size())
            {
                throw CompilationError {
                    .type { CompilationErrorType::InvalidNumberOfArguments },
                    .begin { identifierToken.begin },
                    .end { parenthesisCloseToken.end },
                };
            }

            return Unique<Expression> {
                new FunctionExpression {
                    ::std::move(function),
                    ::std::move(arguments),
                },
            };
        }

        auto paramIter {
            ::std::find(
                signature.parameterNames.begin(), signature.parameterNames.end(), identifier),
        };
        if (paramIter == signature.parameterNames.end())
        {
            throw CompilationError {
                .type { CompilationErrorType::UndefinedParamter },
                .begin { identifierToken.begin },
                .end { identifierToken.end },
            };
        }

        return Unique<Expression> {
            new VariableExpression {
                static_cast<::std::size_t>(
                    ::std::distance(signature.parameterNames.begin(), paramIter)),
            },
        };
    }

    Token const&         constantToken { state.Match(TokenType::Numeric) };
    ::std::u8string_view constant { constantToken.text };

    double value {};
    auto [_, ec] {
        ::std::from_chars(reinterpret_cast<const char*>(constant.data()),
                          reinterpret_cast<const char*>(constant.data() + constant.size()),
                          value),
    };
    if (ec != ::std::errc {})
    {
        throw CompilationError {
            .type { CompilationErrorType::InvalidConstant },
            .begin { constantToken.begin },
            .end { constantToken.end },
        };
    }

    return Unique<Expression> { new ConstantExpression { value } };
}

}

ParsingResult jitdemo::expr::parsing::Parse(Context const&     context,
                                            ::std::span<Token> tokens) noexcept
{
    try
    {
        ParsingState     state { tokens };
        FunctionWithName function { ParseFunc(state, context) };
        return ParsingResult {
            .functionName { function.functionName },
            .function { ::std::move(function.function) },
            .errors {},
        };
    }
    catch (CompilationError const& error)
    {
        return ParsingResult {
            .functionName { u8"" },
            .function { nullptr },
            .errors { error },
        };
    }
}
