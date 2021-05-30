#ifndef TEXTIT_TOKENS_HPP
#define TEXTIT_TOKENS_HPP

enum TokenKind : uint32_t
{
    /* ascii */

    Token_Identifier = 128,
    Token_Keyword,
    Token_Preprocessor,
    Token_String,
    Token_LineComment,
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
        case Token_Preprocessor: return "text_preprocessor"_str;
        case Token_String: return "text_string"_str;
        case Token_LineComment: return "text_line_comment"_str;
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

#define TOKEN_BLOCK_SIZE 512
struct TokenBlock
{
    TokenBlock *next;
    int64_t min_pos;
    int64_t max_pos;
    int64_t count;
    Token tokens[TOKEN_BLOCK_SIZE];
};

#endif /* TEXTIT_TOKENS_HPP */
