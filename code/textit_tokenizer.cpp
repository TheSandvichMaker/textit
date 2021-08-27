function int64_t
CharsLeft(Tokenizer *tok)
{
    AssertSlow(tok->at < tok->end);
    return (int64_t)(tok->end - tok->at);
}

function int64_t
AtPos(Tokenizer *tok)
{
    return tok->at - tok->start;
}

function uint8_t
Peek(Tokenizer *tok, int64_t index = 0)
{
    if (((tok->at + index) >= tok->start) &&
        ((tok->at + index) < tok->end))
    {
        return tok->at[index];
    }
    else
    {
        return 0;
    }
}

function uint8_t
Advance(Tokenizer *tok, int64_t amount = 1)
{
    uint8_t result = Peek(tok);
    tok->at += amount;
    if (tok->at >= tok->end)
    {
        tok->at = tok->end;
    }
    return result;
}

function bool
Match(Tokenizer *tok, uint8_t c)
{
    if (Peek(tok) == c)
    {
        tok->at += 1;
        return true;
    }
    return false;
}

function bool
Match(Tokenizer *tok, String seq)
{
    for (size_t i = 0; i < seq.size; i += 1)
    {
        if (Peek(tok, (int64_t)i) != seq.data[i])
        {
            return false;
        }
    }
    tok->at += seq.size;
    return true;
}

function void
Revert(Tokenizer *tok, Token *t)
{
    uint8_t *new_at = tok->start + t->pos;
    Assert(new_at >= tok->start && new_at < tok->end);
    tok->at = new_at;
}

function bool
MatchOperator(Tokenizer *tok, Token *t, uint8_t a)
{
    bool result = false;

    uint8_t b = Peek(tok, 0);
    uint8_t c = Peek(tok, 1);
    uint8_t d = Peek(tok, 2);

    LanguageSpec *lang = tok->language;

    uint32_t pattern = a|(b << 8)|(c << 16)|(d << 24);
    for (size_t i = 0; i < lang->operator_count; i += 1)
    {
        OperatorSlot *slot = &lang->operators[i];
        if (slot->pattern == (pattern & ((1 << (8*slot->pattern_length)) - 1)))
        {
            t->kind     = slot->kind;
            t->sub_kind = slot->sub_kind;
            Advance(tok, slot->pattern_length - 1);

            result = true;
            break;
        }
    }
    
    return result;
}

function void
ParseString(Tokenizer *tok, Token *t, uint8_t end_char)
{
    // TODO: Make strings not even be tokens, just a flag?
    t->kind = Token_String;
    while (CharsLeft(tok))
    {
        // FIXME: SHOULD EMIT LINE DATA PROPERLY!!
        if ((Peek(tok, 0) == '\r' && Peek(tok, 1) == '\n') ||
            Peek(tok, 0) == '\n')
        {
            if (Peek(tok, -1) != '\\')
            {
                break;
            }

            if (Peek(tok) == '\r')
            {
                Advance(tok);
            }
        }
        else if (Match(tok, end_char))
        {
            break;
        }
        else
        {
            Advance(tok);
        }
    }
}

function void
ParseStandardToken(Tokenizer *tok, Token *t)
{
    uint8_t c = Advance(tok);

    if (MatchOperator(tok, t, c))
    {
        return;
    }

    if (c == '"')
    {
        ParseString(tok, t, '"');
    }
    else if (IsHeadUtf8Byte(c) || IsAlphabeticAscii(c) || c == '_')
    {
        t->kind = Token_Identifier;
        while (CharsLeft(tok) &&
               (IsValidIdentifierAscii(Peek(tok)) || IsUtf8Byte(Peek(tok))))
        {
            Advance(tok);
        }
    }
    else if (IsNumericAscii(c))
    {
        t->kind = Token_Number;
        // TODO: Correct numeric literal rules
        while (CharsLeft(tok) &&
               (IsValidIdentifierAscii(Peek(tok)) ||
                Peek(tok) == '.' ||
                Peek(tok) == '\''))
        {
            Advance(tok);
        }
    }
    else
    {
        t->kind = c;
    }
}

function bool
ParseWhitespace(Tokenizer *tok)
{
    bool result = false;

    // TODO: Handle implicit line endings
    Token *prev_t = tok->prev_token;

    while (CharsLeft(tok))
    {
        CharacterClassFlags c_class = CharacterizeByte(Peek(tok));
        if (HasFlag(c_class, Character_Whitespace))
        {
            Advance(tok);

            if (HasFlag(c_class, Character_VerticalWhitespace))
            {
                prev_t->flags |= TokenFlag_LastInLine;

                tok->new_line = true;

                if (!tok->continue_next_line)
                {
                    tok->in_line_comment = false;
                    // tok->in_preprocessor = false;
                }
                tok->continue_next_line = false;

                //
                // finish old line data
                //

                tok->newline_pos = AtPos(tok) - 1;

                if ((Peek(tok, -1) == '\r') && (Peek(tok, 0) == '\n'))
                {
                    Advance(tok);
                }

                tok->at_line += 1;

                result = true;
                break;
            }
        }
        else
        {
            break;
        }
    }

    return result;
}

function Token *
PushToken(Tokenizer *tok, Token *t)
{
    Buffer *buffer = tok->buffer;

    if (tok->first_token_block == nullptr ||
        tok->first_token_block->token_count >= ArrayCount(tok->first_token_block->tokens))
    {
        TokenBlock *block = AllocateTokenBlock(buffer);
        block->prev = tok->last_token_block;

        if (tok->first_token_block)
        {
            tok->last_token_block = tok->last_token_block->next = block;
        }
        else
        {
            tok->first_token_block = tok->last_token_block = block;
        }
    }

    TokenBlock *block = tok->last_token_block;

    Token *dest = &block->tokens[block->token_count++];
    CopyStruct(t, dest);

    dest->pos = 0;
    return dest;
}

function void
BeginToken(Tokenizer *tok, Token *t)
{
    ZeroStruct(t);
    if (tok->new_line)
    {
        tok->new_line = false;
        t->flags |= TokenFlag_FirstInLine;
    }
    t->pos = tok->at - tok->start;
}

function void
EndToken(Tokenizer *tok, Token *t)
{
    t->length = (int16_t)(tok->at - tok->start - t->pos);

    if (t->kind == Token_Identifier)
    {
        String string = MakeString(t->length, &tok->start[t->pos]);
        StringID id = HashStringID(string);
        if (TokenKind kind = GetTokenKindFromStringID(tok->language, id))
        {
            t->kind = kind;
        }
    }

    switch (t->kind)
    {
        case Token_LineComment:
        {
            tok->in_line_comment = true;
        } break;

        case Token_OpenBlockComment:
        {
            tok->block_comment_count += 1;
        } break;

        case Token_CloseBlockComment:
        {
            if (tok->allow_nested_block_comments)
            {
                tok->block_comment_count -= 1;
            }
            else
            {
                tok->block_comment_count = 0;
            }
        } break;
    }

    if (tok->in_preprocessor)
    {
        t->flags |= TokenFlag_IsPreprocessor;
    }

    if (tok->in_line_comment         || 
        tok->block_comment_count > 0 || 
        t->kind == Token_CloseBlockComment)
    {
        t->flags |= TokenFlag_IsComment;
    }

    tok->prev_token = PushToken(tok, t);
}

function void
TokenizeBasic(Tokenizer *tok)
{
    Token t;
    BeginToken(tok, &t);
    ParseStandardToken(tok, &t);
    EndToken(tok, &t);
}

function void
BeginTokenizeLine(Tokenizer *tok, Buffer *buffer, Range range, LineTokenizeState previous_line_state)
{
    ZeroStruct(tok);
    tok->prev_token = &tok->null_token;
    tok->buffer     = buffer;
    tok->language   = buffer->language;
    tok->start      = buffer->text;
    tok->end        = buffer->text + range.end;
    tok->at         = tok->start + range.start;
    tok->first_token_block = tok->last_token_block = AllocateTokenBlock(buffer);
    tok->new_line   = true;
    tok->line_start = AtPos(tok);

    if (previous_line_state & LineTokenizeState_BlockComment)
    {
        tok->block_comment_count = 1;
    }
}

function void
EndTokenizeLine(Tokenizer *tok, LineData *line)
{
    line->newline_col          = tok->newline_pos - tok->line_start;
    line->first_token_block    = tok->first_token_block;
    line->start_tokenize_state = tok->start_line_state;
    line->end_tokenize_state   = (tok->block_comment_count > 0 ? LineTokenizeState_BlockComment : 0);
    line->first_token_block    = tok->first_token_block;
    line->last_token_block     = tok->last_token_block;
}

function int64_t
TokenizeLine(Buffer *buffer, int64_t pos, LineTokenizeState previous_line_state, LineData *line_data)
{
    Tokenizer tok_, *tok = &tok_;
    BeginTokenizeLine(tok, buffer, MakeRange(pos, buffer->count), previous_line_state);

    while (CharsLeft(tok))
    {
        if (ParseWhitespace(tok))
        {
            break;
        }
        tok->language->Tokenize(tok);
    }

    EndTokenizeLine(tok, line_data);

    return AtPos(tok);
}

function void
TokenizeBuffer(Buffer *buffer)
{
    LineData line_data = {};
    LineData *prev_line_data = &line_data;

    int64_t at = 0;
    while (at < buffer->count)
    {
        int64_t line_start = at;
        at = TokenizeLine(buffer, at, prev_line_data->end_tokenize_state, &line_data);

        LineIndexNode *node = InsertLine(buffer, MakeRange(line_start, at), &line_data);
        prev_line_data = &node->data;
    }
}
