#ifndef TEXTIT_TOKENS_HPP
#define TEXTIT_TOKENS_HPP

enum_flags(uint8_t, TokenKind)
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

        case Token_Identifier:        return "text_identifier"_str;
        case Token_Keyword:           return "text_keyword"_str;
        case Token_FlowControl:       return "text_flowcontrol"_str;
        case Token_Preprocessor:      return "text_preprocessor"_str;
        case Token_String:            return "text_string"_str;
        case Token_Number:            return "text_number"_str;
        case Token_Literal:           return "text_literal"_str;
        case Token_LineComment:       return "text_line_comment"_str;
        case Token_OpenBlockComment:  return "text_line_comment"_str;
        case Token_CloseBlockComment: return "text_line_comment"_str;
        case Token_Type:              return "text_type"_str;
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

function bool
IsNestOpener(TokenKind t)
{
    bool result = ((t == Token_LeftParen) ||
                   (t == Token_LeftScope) ||
                   (t == '<'));
    return result;
}

function bool
IsNestCloser(TokenKind t)
{
    bool result = ((t == Token_RightParen) ||
                   (t == Token_RightScope) ||
                   (t == '>'));
    return result;
}

function TokenKind
GetOtherNestTokenKind(TokenKind kind)
{
    switch (kind)
    {
        case Token_LeftParen:  return Token_RightParen;
        case Token_RightParen: return Token_LeftParen;
        case Token_LeftScope:  return Token_RightScope;
        case Token_RightScope: return Token_LeftScope;
        case '<':              return '>';
        case '>':              return '<';
        INCOMPLETE_SWITCH;
    }
    return Token_None;
}

struct NestHelper
{
    int depth;
    TokenKind opener;
    TokenKind closer;
    TokenKind last_seen_opener;
    TokenKind last_seen_closer;
};

function void
Clear(NestHelper *nests)
{
    ZeroStruct(nests);
}

function bool
IsInNest(NestHelper *nests, TokenKind t, Direction dir)
{
    bool result = nests->depth > 0;

    if (nests->opener)
    {
        if (dir == Direction_Forward && IsNestOpener(t))
        {
            nests->last_seen_opener = t;
        }
        else if (dir == Direction_Backward && IsNestCloser(t))
        {
            if (t != GetOtherNestTokenKind(nests->last_seen_opener))
            {
                // something smells fishy, time to give up
                Clear(nests);
                return false;
            }
        }

        if (t == nests->opener)
        {
            nests->depth += 1;
        }
        else if (t == nests->closer)
        {
            nests->depth -= 1;
            if (nests->depth == 0)
            {
                Clear(nests);
            }
        }
    }
    else
    {
        bool starts_nest = false;
        if (dir == Direction_Forward)  starts_nest = IsNestOpener(t);
        if (dir == Direction_Backward) starts_nest = IsNestCloser(t);
        if (starts_nest)
        {
            nests->opener = t;
            nests->closer = GetOtherNestTokenKind(t);
            nests->depth += 1;

            result = true;
        }
    }

    return result;
}

#endif /* TEXTIT_TOKENS_HPP */
