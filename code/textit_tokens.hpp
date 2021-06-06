#ifndef TEXTIT_TOKENS_HPP
#define TEXTIT_TOKENS_HPP

enum TokenKind : uint32_t
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

    Token_LeftParen,
    Token_RightParen,
    Token_LeftScope,
    Token_RightScope,
};

static inline String
TokenThemeName(TokenKind kind)
{
    switch (kind)
    {
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
    if (kind < 128               ||
        kind == Token_LeftParen  ||
        kind == Token_RightParen ||
        kind == Token_LeftScope  ||
        kind == Token_RightScope)
    {
        return "text_foreground_dim"_str;
    }
    return "text_foreground"_str;
}

typedef uint32_t TokenFlags;
enum TokenFlags_ENUM : TokenFlags
{
    TokenFlag_IsComment = 0x1,
    TokenFlag_IsPreprocessor = 0x2,
};

struct Token
{
    TokenKind kind;
    TokenFlags flags;

    int64_t pos;
    int64_t length;
};

struct TokenBlock;

struct TokenList
{
    TokenBlock *first;
    TokenBlock *last;
};

#define TOKEN_BLOCK_SIZE 512
struct TokenBlock
{
    TokenBlock *next;
    TokenBlock *prev;
    int64_t min_pos;
    int64_t max_pos;
    int64_t count;
    Token tokens[TOKEN_BLOCK_SIZE];
};

struct TokenIterator
{
    TokenBlock *block;
    int64_t index;
    Token *token;
};

static inline TokenIterator
MakeTokenIterator(TokenList *list, int64_t start_pos = 0)
{
    TokenIterator result = {};
    result.block = list->first;
    while (result.block && (start_pos >= result.block->max_pos))
    {
        result.block = result.block->next;
    }
    if (result.block)
    {
        for (;;)
        {
            Token *t = &result.block->tokens[result.index];
            if (start_pos >= (t->pos + t->length))
            {
                result.index += 1;
            }
            else
            {
                break;
            }
        }
        result.token = &result.block->tokens[result.index];
    }
    return result;
}

static inline bool
IsValid(TokenIterator *it)
{
    return !!it->block;
}

static inline Token *
Next(TokenIterator *it)
{
    Token *result = nullptr;
    if (it->block)
    {
        result = &it->block->tokens[it->index];
        it->token = result;

        it->index += 1;
        if (it->index >= it->block->count)
        {
            it->index = 0;
            it->block = it->block->next;
        }
    }
    return result;
}

static inline Token *
Prev(TokenIterator *it)
{
    Token *result = nullptr;
    if (it->block)
    {
        it->index -= 1;

        result = &it->block->tokens[it->index];
        it->token = result;

        if (it->index <= 0)
        {
            it->block = it->block->next;
            if (it->block)
            {
                it->index = it->block->count;
            }
        }
    }
    return result;
}

#endif /* TEXTIT_TOKENS_HPP */
