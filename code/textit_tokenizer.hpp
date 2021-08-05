#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

typedef uint32_t TokenizeFlags;
enum TokenizeFlags_ENUM : TokenizeFlags
{
    Tokenize_AllowNestedBlockComments = 0x1,
};

struct StringMap;

struct Tokenizer
{
    Token null_token;

    VirtualArray<Token> *tokens;
    VirtualArray<LineData> *line_data;

    LanguageSpec *language;

    TokenizeFlags flags;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    int block_comment_count;

    int64_t base;
    uint8_t *start;
    uint8_t *at;
    uint8_t *end;
};

function void TokenizeBuffer(Buffer *buffer);
function void RetokenizeRange(Buffer *buffer, int64_t pos, int64_t delta);

#endif /* TEXTIT_TOKENIZER_HPP */
