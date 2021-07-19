#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

typedef uint32_t TokenizeFlags;
enum TokenizeFlags_ENUM : TokenizeFlags
{
    Tokenize_AllowNestedBlockComments = 0x1,
};

struct StringMap;

struct LanguageSpec
{
    bool allow_nested_block_comments;
    StringMap *idents;
};

struct Tokenizer
{
    Buffer *buffer;
    Token null_token;

    TokenizeFlags flags;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    int block_comment_count;

    uint8_t *start;
    uint8_t *at;
    uint8_t *end;
};

function void TokenizeBuffer(Buffer *buffer);
static PLATFORM_JOB(TokenizeBufferJob);

#endif /* TEXTIT_TOKENIZER_HPP */
