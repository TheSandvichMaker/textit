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
    StringMap *idents;
};

struct Tokenizer
{
    Buffer *buffer;
    Token null_token;

    TokenizeFlags flags;

    LanguageSpec *spec;

    TokenList *tokens;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    int block_comment_count;

    uint8_t *at;
    uint8_t *end;
};

static inline void TokenizeBuffer(Buffer *buffer);
static PLATFORM_JOB(TokenizeBufferJob);

#endif /* TEXTIT_TOKENIZER_HPP */
