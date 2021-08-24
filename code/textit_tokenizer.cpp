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

                int64_t newline_pos = tok->at - tok->start;

                if ((Peek(tok, -1) == '\r') && (Peek(tok, 0) == '\n'))
                {
                    Advance(tok);
                }

                int64_t currently_at = tok->at - tok->start;
                Range line_range = MakeRange(tok->line_start, currently_at);

                LineData *line_data = InsertLine(tok->line_index, line_range);
                line_data->newline_pos = newline_pos;

                line_data->token_index = tok->line_start_token_count;
                line_data->token_count = (int16_t)(tok->tokens->count - line_data->token_index);
                
                tok->at_line += 1;

                //
                // new line data
                //

                tok->line_start = line_range.end;
                tok->line_start_token_count = tok->tokens->count;
            }
        }
        else
        {
            break;
        }
    }

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
            tok->in_line_comment = true);
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

    tok->prev_token = tok->tokens->Push(*t);
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
    }
}

function void
InitializeTokenizer(Tokenizer *tok, Buffer *buffer, Range range, VirtualArray<Token> *tokens)
{
    ZeroStruct(tok);
    tok->prev_token = &tok->null_token;
    tok->tokens     = tokens;
    tok->line_index = &buffer->line_index;
    tok->language   = buffer->language;
    tok->start      = buffer->text + range.start;
    tok->end        = buffer->text + range.end;
    tok->at         = tok->start;
    tok->new_line   = true;

    tok->tokens->Clear();
    tok->tokens->EnsureSpace(1); // what the fuck
}

function void
TokenizeBuffer(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    LanguageSpec *lang = buffer->language;

    Tokenizer tok;
    InitializeTokenizer(&tok, buffer, BufferRange(buffer), &buffer->tokens);

    lang->Tokenize(&tok);

    Assert(tok.at == tok.end);

    int64_t currently_at = tok.at - tok.start;
    Range line_range = MakeRange(tok.line_start, currently_at);

    LineData *final_line_data = InsertLine(tok.line_index, line_range);

    final_line_data->token_index = tok.line_start_token_count;
    final_line_data->token_count = (int16_t)(tok.tokens->count - final_line_data->token_index);
    
    tok.at_line += 1;

    PlatformHighResTime end = platform->GetTime();
    double time = platform->SecondsElapsed(start, end);

    platform->DebugPrint("Tokenized in %fms\n", 1000.0*time);
}

function void
RetokenizeRange(Buffer *buffer, int64_t pos, int64_t delta)
{
    if (delta == 0) return;

    // int64_t first_line = GetLineNumber(buffer, pos);
    // int64_t last_line  = GetLineNumber(buffer, pos + Abs(delta));

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

    Tokenizer tok;
    InitializeTokenizer(&tok, buffer, MakeRange(min_pos, max_pos), &tokens);

    lang->Tokenize(&tok);

    for (size_t i = max_token_index; i < buffer->tokens.count; i += 1)
    {
        Token *t = &buffer->tokens[i];
        t->pos += delta;
    }

    platform->DebugPrint("Retokenize\n");
    platform->DebugPrint("\tpos: %lld, delta: %lld\n", pos, delta);
    platform->DebugPrint("\tedit token index: %u\n", max_token_index);
}

