static inline Token *
PushToken(Tokenizer *tok)
{
    Buffer *buffer = tok->buffer;
    Token *result = &tok->null_token;
    if (buffer->token_count < ArrayCount(buffer->tokens))
    {
        result = &buffer->tokens[buffer->token_count++];
    }
    return result;
}

static inline int64_t
AtPos(Tokenizer *tok)
{
    return (int64_t)(tok->at - tok->buffer->text);
}

static inline int64_t
CharsLeft(Tokenizer *tok)
{
    AssertSlow(tok->at < tok->end);
    return (int64_t)(tok->end - tok->at);
}

static inline void
TokenizeBuffer(Buffer *buffer)
{
    Tokenizer tok_ = {};
    Tokenizer *tok = &tok_;

    tok->buffer = buffer;
    tok->at = buffer->text;
    tok->end = buffer->text + buffer->count;

    buffer->token_count = 0;

    TokenFlags flags = 0;

    while (CharsLeft(tok))
    {
        while (CharsLeft(tok))
        {
            CharacterClassFlags c_class = CharacterizeByte(tok->at[0]);
            if (HasFlag(c_class, Character_Whitespace))
            {
                if (HasFlag(c_class, Character_VerticalWhitespace))
                {
                    flags &= ~TokenFlag_IsComment;
                }
                tok->at += 1;
            }
            else
            {
                break;
            }
        }
        if (!CharsLeft(tok))
        {
            break;
        }

        Token *t = PushToken(tok);
        t->kind = Token_Identifier;

        uint8_t *start = tok->at;

        uint8_t c = tok->at[0];
        tok->at += 1;

        if (IsValidIdentifierAscii(c))
        {
            t->kind = Token_Identifier;
            while (CharsLeft(tok) && !IsWhitespaceAscii(*tok->at))
            {
                tok->at += 1;
            }
        }
        else if (c == '#')
        {
            t->kind = Token_Preprocessor;
            while (CharsLeft(tok) && !IsWhitespaceAscii(*tok->at))
            {
                tok->at += 1;
            }
        }
        else if (c == '"')
        {
            t->kind = Token_String;
            while (CharsLeft(tok) && *tok->at++ != '"');
        }
        else if (c == '/')
        {
            if (CharsLeft(tok) && tok->at[0] == '/')
            {
                t->kind = Token_LineComment;
                flags |= TokenFlag_IsComment;
            }
        }

        uint8_t *end = tok->at;

        t->pos = (int64_t)(start - buffer->text);
        t->length = (int64_t)(end - start);

        t->flags = flags;

        String string = MakeString((size_t)(end - start), start);
        if (AreEqual(string, "if"_str))
        {
            t->kind = Token_Keyword;
        }
        else if (AreEqual(string, "static"_str))
        {
            t->kind = Token_Keyword;
        }
        else if (AreEqual(string, "inline"_str))
        {
            t->kind = Token_Keyword;
        }
    }
}
