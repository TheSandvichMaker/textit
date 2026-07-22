#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

typedef uint32_t TokenizeUserState;
typedef uint32_t TokenizeState;
enum TokenizeState_ENUM : TokenizeState
{
    TokenizeState_ImplicitStatementEndings = 0x1,
    // TODO: TokenizeState_InPreprocessor           = 0x2,
    // TODO: TokenizeState_InLineComment            = 0x4,
    // TODO: TokenizeState_InString                 = 0x8,
};

enum ParseWhitespaceResult
{
    ParseWhitespace_None,
    ParseWhitespace_Horizontal,
    ParseWhitespace_Vertical,
};

struct Tokenizer
{
    void *userdata;

    Token null_token;
    Token *prev_token;
    TokenBlock *first_token_block;
    TokenBlock *last_token_block;

    Buffer *buffer;
    LanguageSpec *language;

    TokenizeState     state;
    TokenizeUserState user_state;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    bool in_string;
    bool in_multiline_string;
    bool allow_nested_block_comments;
    int  block_comment_count;

    int64_t line_start;
    int64_t newline_pos;
    int64_t at_line;
    bool    new_line;
    LineTokenizeState start_line_state;

    int64_t base;
    uint8_t *start;
    uint8_t *at;
    uint8_t *end;
};

function int64_t TokenizeLine(Buffer *buffer, int64_t pos, LineTokenizeState previous_line_state, LineData *line_data);
function void TokenizeBuffer(Buffer *buffer);

function int64_t CharsLeft(Tokenizer *tok);
function int64_t AtPos(Tokenizer *tok);
function uint8_t Peek(Tokenizer *tok, int64_t index = 0);
function uint8_t Advance(Tokenizer *tok, int64_t amount = 1);
function bool Match(Tokenizer *tok, uint8_t c);
function bool Match(Tokenizer *tok, String seq);
function void Revert(Tokenizer *tok, Token *t);
function bool MatchOperator(Tokenizer *tok, Token *t, uint8_t a);
function void ParseString(Tokenizer *tok, Token *t, uint8_t end_char);
function void ParseStandardToken(Tokenizer *tok, Token *t);
function ParseWhitespaceResult ParseWhitespace(Tokenizer *tok);
function Token *PushToken(Tokenizer *tok, Token *t);
function void BeginToken(Tokenizer *tok, Token *t);
function void EndToken(Tokenizer *tok, Token *t);
function void TokenizeBasic(Tokenizer *tok, Token *t);
function void BeginTokenizeLine(Arena *arena, Tokenizer *tok, Buffer *buffer, Range range, LineTokenizeState previous_line_state);
function void EndTokenizeLine(Tokenizer *tok, LineData *line, LineTokenizeState previous_line_state);
function int64_t TokenizeLine(Buffer *buffer, int64_t pos, LineTokenizeState previous_line_state, LineData *line_data);
function void TokenizeBuffer(Buffer *buffer);

#endif /* TEXTIT_TOKENIZER_HPP */
