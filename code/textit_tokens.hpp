#ifndef TEXTIT_TOKENS_HPP
#define TEXTIT_TOKENS_HPP

enum TokenKind : uint8_t
{
    Token_None = 0,

    /* ascii */

    Token_Identifier = 128,
    Token_Keyword,
    Token_FlowControl,
    Token_Preprocessor,
    Token_String,
    Token_Number,
    Token_Literal,
    Token_LineComment,
    Token_OpenBlockComment,
    Token_CloseBlockComment,
    Token_Type,

    Token_FirstOperator,
    Token_Equals = Token_FirstOperator,
    Token_Assignment,
    Token_NotEquals,
    Token_LeftParen,
    Token_RightParen,
    Token_LeftScope,
    Token_RightScope,
    Token_LastOperator = Token_RightScope,

    // "ghost" tokens, mark-up of the code for convenience that doesn't match actual physical characters

    Token_LineEnd,

    Token_COUNT,
};

function String
TokenThemeName(TokenKind kind)
{
    switch (kind)
    {
        INCOMPLETE_SWITCH;

        case Token_Identifier: return "text_identifier"_str;
        case Token_Keyword: return "text_keyword"_str;
        case Token_FlowControl: return "text_flowcontrol"_str;
        case Token_Preprocessor: return "text_preprocessor"_str;
        case Token_String: return "text_string"_str;
        case Token_Number: return "text_number"_str;
        case Token_Literal: return "text_literal"_str;
        case Token_LineComment: return "text_line_comment"_str;
        case Token_OpenBlockComment: return "text_line_comment"_str;
        case Token_CloseBlockComment: return "text_line_comment"_str;
        case Token_Type: return "text_type"_str;
    }
    // single character tokens / scopes are dim
    if (kind < 128 ||
        (kind >= Token_FirstOperator &&
         kind <= Token_LastOperator))
    {
        return "text_foreground_dim"_str;
    }
    return "text_foreground"_str;
}

enum_flags(int, TokenFlags)
{
    TokenFlag_IsComment      = 0x1,
    TokenFlag_IsPreprocessor = 0x2,
    TokenFlag_FirstInLine    = 0x4,
    TokenFlag_LastInLine     = 0x8,
};

struct Token
{
    TokenKind kind;
    TokenFlags flags;

    int64_t pos;
    int64_t length;
};

struct TokenIterator
{
    Token *tokens;
    Token *tokens_end;
    Token *token;
};

function bool
IsValid(TokenIterator *it)
{
    return !!it->token;
}

function Token *
Next(TokenIterator *it)
{
    Token *result = nullptr;
    if (it->token < it->tokens_end)
    {
        result = it->token++;
    }
    if (it->token >= it->tokens_end)
    {
        it->token = nullptr;
    }
    return result;
}

function Token *
Prev(TokenIterator *it)
{
    Token *result = nullptr;
    if (it->token > it->tokens)
    {
        result = --it->token;
    }
    if (it->token <= it->tokens)
    {
        it->token = nullptr;
    }
    return result;
}

#endif /* TEXTIT_TOKENS_HPP */
