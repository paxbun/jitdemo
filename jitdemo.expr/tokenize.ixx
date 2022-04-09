// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <string_view>
#include <variant>
#include <vector>

export module jitdemo.expr.tokenize;

import jitdemo.expr.token;
import jitdemo.expr.compilation_error;

export namespace jitdemo::expr
{

struct TokenizationResult
{
    ::std::vector<Token>            tokens;
    ::std::vector<CompilationError> errors;
};

TokenizationResult Tokenize(::std::u8string_view source) noexcept;

}

module : private;

#include <cctype>

using ::jitdemo::expr::CompilationError;
using ::jitdemo::expr::Token;
using ::jitdemo::expr::TokenizationResult;
using ::jitdemo::expr::TokenType;

namespace
{

using Tokenizer = bool (*)(::std::u8string_view::iterator& begin,
                           ::std::u8string_view::iterator  end,
                           TokenType&                      type) noexcept;

template <char8_t Character, TokenType Type>
bool SingleCharacterTokenizer(::std::u8string_view::iterator& begin,
                              ::std::u8string_view::iterator  _,
                              TokenType&                      type) noexcept
{
    if (*begin != Character)
        return false;

    ++begin;
    type = Type;

    return true;
}

Tokenizer tokenizers[] {
    SingleCharacterTokenizer<u8'(', TokenType::ParenthesisOpen>,
    SingleCharacterTokenizer<u8')', TokenType::ParenthesisClose>,
    SingleCharacterTokenizer<u8'+', TokenType::Plus>,
    SingleCharacterTokenizer<u8'-', TokenType::Minus>,
    SingleCharacterTokenizer<u8'*', TokenType::Asterisk>,
    SingleCharacterTokenizer<u8'/', TokenType::Slash>,
    SingleCharacterTokenizer<u8'^', TokenType::Caret>,
    SingleCharacterTokenizer<u8',', TokenType::Comma>,
    [](::std::u8string_view::iterator& begin,
       ::std::u8string_view::iterator  end,
       TokenType&                      type) noexcept -> bool {
        while (begin != end and ::std::isalpha(*begin)) ++begin;
        if (begin == end)
            return false;

        type = TokenType::Identifier;
        return true;
    },
    [](::std::u8string_view::iterator& begin,
       ::std::u8string_view::iterator  end,
       TokenType&                      type) noexcept -> bool {
        while (begin != end and ::std::isspace(*begin)) ++begin;
        if (begin == end)
            return false;

        type = TokenType::Whitespace;
        return true;
    },
};

}

TokenizationResult jitdemo::expr::Tokenize(::std::u8string_view source) noexcept
{
    ::std::vector<Token> tokens;
    ::std::vector<bool>  invalidChars(source.size(), false);

    {
        auto const begin { source.begin() };
        auto const end { source.end() };

        bool atInvalidChar { false };
        auto invalidCharBegin { begin };

        auto current { begin };
        while (current != end)
        {
            for (auto tokenizer : tokenizers)
            {
                auto nextCurrent { current };
                if (TokenType type; tokenizer(nextCurrent, end, type))
                {
                    tokens.push_back(Token {
                        .type { type },
                        .begin { static_cast<uint32_t>(::std::distance(begin, current)) },
                        .end { static_cast<uint32_t>(::std::distance(begin, nextCurrent)) },
                        .text { ::std::u8string_view { current, nextCurrent } },
                    });

                    current = nextCurrent;
                    goto tokenized;
                }
            }

notTokenized:
            invalidChars[::std::distance(begin, current)] = true;
            ++current;
            continue;

tokenized:
            continue;
        }
    }

    ::std::vector<CompilationError> errors;
    {
        auto const begin { invalidChars.begin() };
        auto const end { invalidChars.end() };

        auto current { begin };
        while (current != end)
        {
            while (current != end and not *current) ++current;
            if (current == end)
                break;

            auto newCurrent { current };
            while (newCurrent != end and *newCurrent) ++newCurrent;

            errors.push_back(CompilationError {
                .type { CompilationErrorType::InvalidCharacter },
                .begin { static_cast<uint32_t>(::std::distance(begin, current)) },
                .end { static_cast<uint32_t>(::std::distance(begin, newCurrent)) },
            });
            current = newCurrent;
        }
    }

    return TokenizationResult {
        .tokens { ::std::move(tokens) },
        .errors { ::std::move(errors) },
    };
}