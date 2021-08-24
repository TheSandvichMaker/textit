#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

typedef uint32_t TokenizeUserState;
typedef uint32_t TokenizeState;
enum TokenizeState_ENUM : TokenizeState
{
    TokenizeState_ImplicitStatementEndings = 0x1,
};

struct StringMap;

struct Tokenizer
{
    Token null_token;
    Token *prev_token;
    VirtualArray<Token> *tokens;

    LineIndex *line_index;

    LanguageSpec *language;

    TokenizeState     state;
    TokenizeUserState user_state;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    bool allow_nested_block_comments;
    int  block_comment_count;

    int64_t  line_start;
    uint32_t line_start_token_count;

    int64_t at_line;
    bool    new_line;

    int64_t base;
    uint8_t *start;
    uint8_t *at;
    uint8_t *end;
};

function void TokenizeBuffer(Buffer *buffer);
function void RetokenizeRange(Buffer *buffer, int64_t pos, int64_t delta);

#endif /* TEXTIT_TOKENIZER_HPP */
