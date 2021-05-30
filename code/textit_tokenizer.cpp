static inline TokenBlock *
AllocateTokenBlock(void)
{
    if (!editor_state->first_free_token_block)
    {
        editor_state->first_free_token_block = PushStructNoClear(&editor_state->transient_arena, TokenBlock);
        editor_state->first_free_token_block->next = nullptr;
    }
    TokenBlock *result = editor_state->first_free_token_block;
    editor_state->first_free_token_block = result->next;

    ZeroStruct(result);
    result->min_pos = INT64_MAX;
    result->max_pos = INT64_MIN;
    return result;
}

static inline void
PushToken(Tokenizer *tok, const Token &t)
{
    Buffer *buf = tok->buffer;

    if (!buf->first_token_block ||
        buf->last_token_block->count >= ArrayCount(buf->last_token_block->tokens))
    {
        TokenBlock *block = AllocateTokenBlock();
        SllQueuePush(buf->first_token_block, buf->last_token_block, block);
    }
    TokenBlock *block = tok->buffer->last_token_block;

    if (t.pos < block->min_pos) block->min_pos = t.pos;
    if (t.pos + t.length > block->max_pos) block->max_pos = t.pos + t.length;

    buf->token_count += 1;
    block->tokens[block->count++] = t;
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

static inline uint8_t
Peek(Tokenizer *tok, int64_t index = 0)
{
    if (((tok->at + index) >= tok->buffer->text) &&
        ((tok->at + index) < tok->end))
    {
        return tok->at[index];
    }
    else
    {
        return 0;
    }
}

static inline void
TokenizeBuffer(Buffer *buffer)
{
    Tokenizer tok_ = {};
    Tokenizer *tok = &tok_;

    tok->buffer = buffer;
    tok->at = buffer->text;
    tok->end = buffer->text + buffer->count;

    if (buffer->last_token_block)
    {
        buffer->last_token_block->next = editor_state->first_free_token_block;
    }
    editor_state->first_free_token_block = buffer->first_token_block;
    
    buffer->token_count = 0;
    buffer->first_token_block = nullptr;
    buffer->last_token_block = nullptr;

    while (CharsLeft(tok))
    {
        while (CharsLeft(tok))
        {
            CharacterClassFlags c_class = CharacterizeByte(tok->at[0]);
            if (HasFlag(c_class, Character_Whitespace))
            {
                if (HasFlag(c_class, Character_VerticalWhitespace))
                {
                    if (!tok->continue_next_line)
                    {
                        tok->in_line_comment = false;
                        tok->in_preprocessor = false;
                    }
                    tok->continue_next_line = false;
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
        
        tok->continue_next_line = false;

        Token t = {};
        t.kind = Token_Identifier;

        uint8_t *start = tok->at;

        uint8_t c = tok->at[0];
        tok->at += 1;

        switch (c)
        {
            default:
            {
                if (IsAlphabeticAscii(c) || c == '_')
                {
                    t.kind = Token_Identifier;
                    while (CharsLeft(tok) && IsValidIdentifierAscii(Peek(tok)))
                    {
                        tok->at += 1;
                    }
                }
                else if (IsNumericAscii(c))
                {
                    t.kind = Token_Number;
                    while (CharsLeft(tok) && (IsValidIdentifierAscii(Peek(tok)) || Peek(tok) == '.'))
                    {
                        tok->at += 1;
                    }
                }
                else
                {
                    t.kind = (TokenKind)c;
                }
            } break;

            case '#':
            {
                t.kind = Token_Preprocessor;
                while (CharsLeft(tok) && !IsWhitespaceAscii(Peek(tok)))
                {
                    tok->at += 1;
                }
                tok->in_preprocessor = true;
            } break;

            case '"':
            {
                t.kind = Token_String;
                while (CharsLeft(tok) && *tok->at++ != '"');
            } break;

            case '/':
            {
                if (CharsLeft(tok) && Peek(tok) == '/')
                {
                    t.kind = Token_LineComment;
                    tok->in_line_comment = true;
                }
                if (CharsLeft(tok) && Peek(tok) == '*')
                {
                    t.kind = Token_OpenBlockComment;
                    tok->at += 1;
                    tok->block_comment_count += 1;
                }
            } break;

            case '*':
            {
                if (CharsLeft(tok) && Peek(tok) == '/')
                {
                    tok->at += 1;
                    if (tok->block_comment_count > 0)
                    {
                        tok->block_comment_count -= 1;
                    }
                    t.kind = Token_CloseBlockComment;
                    t.flags |= TokenFlag_IsComment;
                }
                else
                {
                    t.kind = (TokenKind)c;
                }
            } break;

            case '\\':
            {
                t.kind = (TokenKind)c;
                tok->continue_next_line = true;
            } break;

            case '(': { t.kind = Token_LeftParen; } break;
            case ')': { t.kind = Token_RightParen; } break;
            case '{': { t.kind = Token_LeftScope; } break;
            case '}': { t.kind = Token_RightScope; } break;
        }

        if (tok->in_line_comment || tok->block_comment_count > 0)
        {
            t.flags |= TokenFlag_IsComment;
        }
        if (tok->in_preprocessor)
        {
            t.flags |= TokenFlag_IsPreprocessor;
        }

        uint8_t *end = tok->at;

        t.pos = (int64_t)(start - buffer->text);
        t.length = (int64_t)(end - start);

        String string = MakeString((size_t)(end - start), start);
        if (t.kind == Token_Identifier)
        {
            for (size_t i = 0; i < ArrayCount(cpp_keywords); i += 1)
            {
                if (AreEqual(string, cpp_keywords[i]))
                {
                    t.kind = Token_Keyword;
                    break;
                }
            }
        }

        if (t.kind == Token_Identifier)
        {
            for (size_t i = 0; i < ArrayCount(cpp_flow_control_keywords); i += 1)
            {
                if (AreEqual(string, cpp_flow_control_keywords[i]))
                {
                    t.kind = Token_FlowControl;
                    break;
                }
            }
        }

        if (t.kind == Token_Identifier)
        {
            for (size_t i = 0; i < ArrayCount(cpp_literals); i += 1)
            {
                if (AreEqual(string, cpp_literals[i]))
                {
                    t.kind = Token_Literal;
                    break;
                }
            }
        }

        if (t.kind == Token_Identifier)
        {
            for (size_t i = 0; i < ArrayCount(cpp_keywords); i += 1)
            {
                if (AreEqual(string, cpp_keywords[i]))
                {
                    t.kind = Token_Keyword;
                    break;
                }
            }
        }

        PushToken(tok, t);
    }
}
