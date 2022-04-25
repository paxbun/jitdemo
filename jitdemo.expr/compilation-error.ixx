// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cstdint>

export module jitdemo.expr.compilation_error;

export namespace jitdemo::expr
{

enum class CompilationErrorType
{
    /**
     * Unexpected exception has been thrown.
     */
    UnknownError,

    /**
     * An unexpected character has been encountered during tokenization.
     */
    InvalidCharacter,

    /**
     * An unexpected token has been encountered when parsing.
     */
    UnexpectedToken,

    /**
     * There is no function with the given name in the current session.
     */
    UndefinedFunction,

    /**
     * There is no parameter with the given name in the current function.
     */
    UndefinedParamter,

    /**
     * The parser failed to convert the given constant to a floating-point value.
     */
    InvalidConstant,
};

/**
 * `CompilationError` represents a single compilation error.
 */
struct CompilationError
{
    /**
     * The type of the error.
     */
    CompilationErrorType type;

    /**
     * The index of the starting character (inclusive).
     */
    ::std::uint32_t begin;

    /**
     * The index of the ending character (exclusive).
     */
    ::std::uint32_t end;
};

}
