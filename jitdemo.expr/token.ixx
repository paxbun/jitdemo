// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cstdint>
#include <string_view>

export module jitdemo.expr.token;

export namespace jitdemo::expr
{

enum class TokenType
{
    /**
     * The left side of a round parenthesis ("(").
     */
    ParenthesisOpen,

    /**
     * The right side of a round parenthesis (")").
     */
    ParenthesisClose,

    /**
     * The addition sign ("+").
     */
    Plus,

    /**
     * The subtraction sign ("-").
     */
    Minus,

    /**
     * The multiplication sign ("*").
     */
    Asterisk,

    /**
     * The division sign ("/").
     */
    Slash,

    /**
     * The power sign ("^").
     */
    Caret,

    /**
     * A name of a function parameter or a function ([a-zA-Z]+).
     */
    Identifier,

    /**
     * A string of whitespaces of an arbitrary length (\s+).
     */
    Whitespace,
};

/**
 * `Token` represents a single token generated by the tokenizer.
 */
struct Token
{
    /**
     * The type of the token.
     */
    TokenType type;

    /**
     * The index of the starting character (inclusive).
     */
    std::uint32_t begin;

    /**
     * The index of the ending character (exclusive).
     */
    std::uint32_t end;

    /**
     * The underlying text of this token.
     */
    std::u8string_view text;
};

}