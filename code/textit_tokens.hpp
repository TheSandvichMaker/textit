#ifndef TEXTIT_TOKENS_HPP
#define TEXTIT_TOKENS_HPP

struct Buffer;

typedef uint8_t TokenKind;
enum TokenKind_ENUM : TokenKind
{
    Token_None = 0,

    /* ascii */

    Token_Identifier = 128,
    Token_Whitespace,
    Token_Keyword,
    Token_FlowControl,
    Token_Label,
    Token_Preprocessor,
    Token_String, // not produced by tokenizer, but can be generated with helper functions
    Token_StartString,
    Token_EndString,
    Token_Number,
    Token_Literal,
    Token_CharacterLiteral,
    Token_Function,
    Token_LineComment,
    Token_OpenBlockComment,
    Token_CloseBlockComment,
    Token_Type,
    Token_LineContinue,

    Token_Operator,
    Token_FirstOperator = Token_Operator, // Token_Operator comes first because then it shows up in debuggers as I want
    Token_LeftParen,
    Token_RightParen,
    Token_LeftScope,
    Token_RightScope,
    Token_LastOperator = Token_RightScope,

    Token_Custom, // check sub kind

    Token_COUNT,
};

function StringID
GetBaseTokenThemeID(TokenKind kind)
{
    switch (kind)
    {
        INCOMPLETE_SWITCH;

        case Token_Identifier:        return "text_identifier"_id;
        case Token_Keyword:           return "text_keyword"_id;
        case Token_FlowControl:       return "text_flowcontrol"_id;
        case Token_Label:             return "text_label"_id;
        case Token_Preprocessor:      return "text_preprocessor"_id;
        case Token_String:            return "text_string"_id;
        case Token_StartString:       return "text_string"_id;
        case Token_EndString:         return "text_string"_id;
        case Token_Number:            return "text_number"_id;
        case Token_Literal:           return "text_literal"_id;
        case Token_CharacterLiteral:  return "text_literal"_id;
        case Token_Function:          return "text_function"_id;
        case Token_LineComment:       return "text_comment"_id;
        case Token_OpenBlockComment:  return "text_comment"_id;
        case Token_CloseBlockComment: return "text_comment"_id;
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

typedef uint8_t TokenFlags;
enum TokenFlags_ENUM : TokenFlags
{
    TokenFlag_IsComment        = 0x1,
    TokenFlag_IsPreprocessor   = 0x2,
    TokenFlag_NonParticipating = 0x4,
    TokenFlag_FirstInLine      = 0x8,
    TokenFlag_LastInLine       = 0x10,
    TokenFlag_PartOfString     = 0x20,
};

typedef uint8_t TokenSubKind;

struct Token
{
    TokenKind    kind;               // 1
    TokenSubKind sub_kind;           // 2
    TokenFlags   flags;              // 3
    uint8_t      pad0;               // 4
    int16_t      length;             // 6 (it's completely realistic to limit the length of tokens to something lower like 16 bits, but I should make sure I handle degenerate cases)
    int8_t       inner_start_offset; // 7 (e.g. for strings, the offset from the start of the token to the start of the contents of the string)
    int8_t       inner_end_offset;   // 8 (e.g. for strings, the offset from the end of the token to the end of the contents of the string)
    int64_t      pos;                // 16

    operator bool() { return !!kind; }
};

function Range
GetTokenRange(Token token)
{
    Range result;
    result.start = token.pos;
    result.end   = token.pos + token.length;
    return result;
}

function bool
IsInTokenRange(Token token, int64_t pos)
{
    return pos >= token.pos && pos < token.pos + token.length;
}

function Range
GetInnerRange(Token token)
{
    Range result;
    result.start = token.pos + token.inner_start_offset;
    result.end   = token.pos + token.length - token.inner_end_offset;
    return result;
}

#define TOKEN_BLOCK_FREE_TAG -1

struct TokenBlock
{
    TokenBlock *next;
    TokenBlock *prev;
    int64_t token_count;
    Token tokens[16];
};

struct TokenLocator
{
    TokenBlock *block;
    int64_t     index;
    int64_t     pos;
};

function bool
IsValid(TokenLocator locator)
{
    return locator.block && locator.index >= 0 && locator.index < locator.block->token_count;
}

function Token
GetToken(TokenLocator locator)
{
    TokenBlock *block = locator.block;
    int64_t     index = locator.index;
    int64_t     pos   = locator.pos;

    Token result = {};
    if (block && index >= 0 && index < block->token_count)
    {
        result = block->tokens[index];
        result.pos = pos;
    }
    return result;
}

function bool ValidateTokenLocatorIntegrity(Buffer *buffer, TokenLocator locator);

struct TokenIterator
{
#if TEXTIT_SLOW
    int64_t        iteration_index;
#endif
    TokenBlock    *block;
    int64_t        index;
    Token          token;
};

typedef int GetTokenFlags;
enum GetTokenFlags_ENUM : GetTokenFlags
{
    GetToken_EncloseStrings = 0x1,
};

struct NestHelper
{
    NestHelper *next;

    int depth;
    TokenKind opener;
    TokenKind closer;
    TokenKind last_seen_opener;
    TokenKind last_seen_closer;
};

enum NestResult
{
    NestResult_NotInNest = 0,
    NestResult_InNest,
    NestResult_EnteringNest,
    NestResult_ExitingNest,
};

function bool
IsNestOpener(TokenKind t)
{
    bool result = ((t == Token_LeftParen) ||
                   (t == Token_LeftScope) ||
                   (t == '<') ||
                   (t == '['));
    return result;
}

function bool
IsNestCloser(TokenKind t)
{
    bool result = ((t == Token_RightParen) ||
                   (t == Token_RightScope) ||
                   (t == '>') ||
                   (t == ']'));
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
        case '[':              return ']';
        case ']':              return '[';
        INCOMPLETE_SWITCH;
    }
    return Token_None;
}

#endif /* TEXTIT_TOKENS_HPP */
