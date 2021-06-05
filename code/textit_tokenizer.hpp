#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

typedef uint32_t TokenizeFlags;
enum TokenizeFlags_ENUM : TokenizeFlags
{
    Tokenize_AllowNestedBlockComments = 0x1,
};

struct Tokenizer
{
    Buffer *buffer;
    Token null_token;

    TokenizeFlags flags;

    TokenList *tokens;

    bool continue_next_line;
    bool in_line_comment;
    bool in_preprocessor;
    int block_comment_count;

    uint8_t *at;
    uint8_t *end;
};

static const String cpp_keywords[] =
{
    "static"_str, "inline"_str, "operator"_str, "volatile"_str, "unsigned"_str,
    "signed"_str, "register"_str, "extern"_str, "const"_str, "struct"_str, "enum"_str,
    "class"_str, "public"_str, "private"_str, "return"_str,
};

static const String cpp_flow_control_keywords[] =
{
    "case"_str, "continue"_str, "break"_str, "if"_str, "else"_str, "for"_str, "switch"_str,
    "while"_str, "do"_str,
};

static const String cpp_literals[] =
{
    "true"_str, "false"_str,
};

static const String cpp_builtin_types[] =
{
    "void"_str, "char"_str, "short"_str, "int"_str, "long"_str, "float"_str, "double"_str,
    "int8_t"_str, "int16_t"_str, "int32_t"_str, "int64_t"_str,
    "uint8_t"_str, "uint16_t"_str, "uint32_t"_str, "uint64_t"_str,
};

static inline void TokenizeBuffer(Buffer *buffer);
static PLATFORM_JOB(TokenizeBufferJob);

#endif /* TEXTIT_TOKENIZER_HPP */
