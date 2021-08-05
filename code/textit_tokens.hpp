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
    Token_CharacterLiteral,
    Token_Function,
    Token_LineComment,
    Token_OpenBlockComment,
    Token_CloseBlockComment,
    Token_Type,

    Token_FirstOperator,
    Token_Operator = Token_FirstOperator,
    Token_LeftParen,
    Token_RightParen,
    Token_LeftScope,
    Token_RightScope,
    Token_LastOperator = Token_RightScope,

    Token_COUNT,
};

function uint64_t
TokenThemeIndex(TokenKind kind)
{
    switch (kind)
    {
        INCOMPLETE_SWITCH;

        case Token_Identifier:        return "text_identifier"_id;
        case Token_Keyword:           return "text_keyword"_id;
        case Token_FlowControl:       return "text_flowcontrol"_id;
        case Token_Preprocessor:      return "text_preprocessor"_id;
        case Token_String:            return "text_string"_id;
        case Token_Number:            return "text_number"_id;
        case Token_Literal:           return "text_literal"_id;
        case Token_CharacterLiteral:  return "text_literal"_id;
        case Token_Function:          return "text_function"_id;
        case Token_LineComment:       return "text_line_comment"_id;
        case Token_OpenBlockComment:  return "text_line_comment"_id;
        case Token_CloseBlockComment: return "text_line_comment"_id;
        case Token_Type:              return "text_type"_id;
    }
    // single character tokens / scopes are dim
    if (kind < 128 ||
        (kind >= Token_FirstOperator &&
         kind <= Token_LastOperator))
    {
        return "text_foreground_dim"_id;
    }
    return "text_foreground"_id;
}

enum_flags(uint8_t, TokenFlags)
{
    TokenFlag_IsComment      = 0x1,
    TokenFlag_IsPreprocessor = 0x2,
    TokenFlag_FirstInLine    = 0x4,
    TokenFlag_LastInLine     = 0x8,
};

struct Token
{
    TokenKind kind;   // 2
    TokenFlags flags; // 4
    int32_t length;   // 8
    int64_t pos;      // 16
};

struct TokenIterator
{
    Token *token;
    Token *tokens;
    Token *tokens_end;
};

function bool
IsValid(TokenIterator *it)
{
    return !!it->token;
}

function void
Rewind(TokenIterator *it, Token *t)
{
    Assert(t >= it->tokens && t < it->tokens_end);
    it->token = t;
}

function Token *
PeekNext(TokenIterator *it, int offset = 0)
{
    Token *result = nullptr;
    if ((it->token + offset) < it->tokens_end)
    {
        result = it->token + offset;
    }
    return result;
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
PeekPrev(TokenIterator *it, int offset = 0)
{
    Token *result = nullptr;
    if ((it->token - offset - 1) >= it->tokens)
    {
        result = it->token - offset - 1;
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
