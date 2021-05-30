#ifndef TEXTIT_TOKENIZER_HPP
#define TEXTIT_TOKENIZER_HPP

struct Tokenizer
{
    Buffer *buffer;
    Token null_token;

    uint8_t *at;
    uint8_t *end;
};

static inline void TokenizeBuffer(Buffer *buffer);

#endif /* TEXTIT_TOKENIZER_HPP */
