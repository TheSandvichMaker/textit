function LineData *
AllocateLineData(void)
{
    LineData *result = PushStruct(&editor->transient_arena, LineData);
    return result;
}

function Token *
PushToken(Tokenizer *tok, Token *t)
{
    Token *result = tok->tokens->Push(*t);
    tok->prev_token = result;
    return result;
}

function int64_t
CharsLeft(Tokenizer *tok)
{
    AssertSlow(tok->at < tok->end);
    return (int64_t)(tok->end - tok->at);
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
            Advance(tok, slot->pattern_length);

            result = true;
            break;
        }
    }
    
    return result;
}

function void
ParseString(Tokenizer *tok, Token *t, uint8_t end_char)
{
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
        t->kind = Token_Operator;
    }
}

function bool
ParseWhitespace(Tokenizer *tok)
{
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

                LineData *line_data = &(*tok->line_data)[tok->at_line];
                line_data->newline_pos = tok->at - tok->start - 1;

                if ((Peek(tok, -1) == '\r') && (Peek(tok, 0) == '\n'))
                {
                    Advance(tok);
                }

                line_data->range.end   = tok->at - tok->start;
                line_data->token_count = (int16_t)(tok->tokens->count - line_data->token_index);
                
                tok->at_line += 1;
                Assert(tok->at_line < (int64_t)tok->line_data->capacity);

                //
                // new line data
                //

                LineData *new_line = tok->line_data->Push();
                ZeroStruct(new_line);

                new_line->range.start = tok->at - tok->start;
                new_line->token_index = (int32_t)tok->tokens->count;
            }
        }
        else
        {
            break;
        }
    }
    
    tok->continue_next_line = false;

    bool result = !CharsLeft(tok);
    return result;
}

function void
BeginToken(Tokenizer* tok, Token *t)
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

    if (tok->in_line_comment || tok->block_comment_count > 0)
    {
        t->flags |= TokenFlag_IsComment;
    }

    t->length = (int16_t)(tok->at - tok->start - t->pos);
}

function void
TokenizeBasic(Tokenizer *tok)
{
    while (CharsLeft(tok))
    {
        if (!ParseWhitespace(tok))
        {
            break;
        }
        
        Token t;
        BeginToken(tok, &t);
        ParseStandardToken(tok, &t);

        EndToken(tok, &t);
        PushToken(tok, &t);
    }

    LineData *end_line_data = &(*tok->line_data)[tok->at_line];
    end_line_data->range.end = tok->end - tok->start;
}

function void
TokenizeBuffer(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    LanguageSpec *lang = buffer->language;

    Tokenizer tok = {};
    tok.prev_token = &tok.null_token;
    tok.tokens     = &buffer->tokens;
    tok.line_data  = &buffer->line_data;
    tok.language   = buffer->language;
    tok.start      = buffer->text;
    tok.end        = buffer->text + buffer->count;
    tok.at         = tok.start;
    tok.new_line   = true;
    tok.line_data->Clear();
    tok.tokens->Clear();
    tok.tokens->EnsureSpace(1); // TODO: jank alert? forcing the allocation so that line_data->tokens below will not be null

    LineData *line_data = tok.line_data->Push();
    ZeroStruct(line_data);

    if (lang->Tokenize)
    {
        lang->Tokenize(&tok);
    }
    else
    {
        TokenizeBasic(&tok);
    }

    PlatformHighResTime end = platform->GetTime();
    double time = platform->SecondsElapsed(start, end);

    platform->DebugPrint("Tokenized in %fms\n", 1000.0*time);
}

function void
RetokenizeRange(Buffer *buffer, int64_t pos, int64_t delta)
{
    if (delta == 0) return;

    LanguageSpec *lang = buffer->language;

    uint32_t min_token_index = FindTokenIndexForPos(buffer, pos);
    uint32_t max_token_index = FindTokenIndexForPos(buffer, pos + Abs(delta));

    if (buffer->tokens[max_token_index].pos < pos + delta)
    {
        max_token_index += 1;
    }

    int64_t min_pos = Min(pos, buffer->tokens[min_token_index].pos);
    int64_t max_pos = buffer->tokens[max_token_index].pos;

    VirtualArray<Token> tokens = {};
    tokens.SetCapacity(4'000'000);
    defer { tokens.Release(); };

    VirtualArray<LineData> line_data = {};
    line_data.SetCapacity(1'000'000);
    defer { line_data.Release(); };

    Tokenizer tok = {};
    tok.tokens    = &tokens;
    tok.line_data = &line_data;
    tok.language  = buffer->language;
    tok.base      = min_pos;
    tok.start     = buffer->text + min_pos;
    tok.end       = buffer->text + max_pos;
    tok.at        = tok.start;
    if (lang->Tokenize)
    {
        lang->Tokenize(&tok);
    }
    else
    {
        TokenizeBasic(&tok);
    }

    for (size_t i = max_token_index; i < buffer->tokens.count; i += 1)
    {
        Token *t = &buffer->tokens[i];
        t->pos += delta;
    }

    platform->DebugPrint("Retokenize\n");
    platform->DebugPrint("\tpos: %lld, delta: %lld\n", pos, delta);
    platform->DebugPrint("\tedit token index: %u\n", max_token_index);
}

