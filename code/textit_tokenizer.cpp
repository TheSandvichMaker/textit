function LineData *
AllocateLineData(void)
{
    LineData *result = PushStruct(&editor->transient_arena, LineData);
    return result;
}

function Token *
PushToken(Tokenizer *tok, const Token &t)
{
    return tok->tokens->Push(t);
}

function int64_t
AtPos(Tokenizer *tok)
{
    return (int64_t)(tok->at - tok->start);
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
Tokenize(Tokenizer *tok)
{
    tok->at = tok->start;

    tok->line_data->Clear();
    tok->tokens->Clear();
    tok->tokens->EnsureSpace(1); // TODO: jank alert? forcing the allocation so that line_data->tokens below will not be null

    LanguageSpec *language = tok->language;

    int line_count = 1;
    int tokens_at_start_of_line = 0;

    bool at_newline = true;

    LineData *line_data = tok->line_data->Push();
    ZeroStruct(line_data);

    line_data->token_index = 0;

    Token *prev_token = nullptr;

    while (CharsLeft(tok))
    {
        while (CharsLeft(tok))
        {
            CharacterClassFlags c_class = CharacterizeByte(tok->at[0]);
            if (HasFlag(c_class, Character_Whitespace))
            {
                tok->at += 1;

                if (HasFlag(c_class, Character_VerticalWhitespace))
                {
                    if (prev_token)
                    {
                        prev_token->flags |= TokenFlag_LastInLine;
                    }

                    at_newline = true;

                    if (!tok->continue_next_line)
                    {
                        tok->in_line_comment = false;
                        tok->in_preprocessor = false;
                    }
                    tok->continue_next_line = false;

                    //
                    // finish old line data
                    //

                    line_data->newline_pos = tok->at - tok->start - 1;

                    if ((tok->at[-1] == '\r') && (tok->at[0] == '\n'))
                    {
                        tok->at += 1;
                    }

                    line_data->range.end = tok->at - tok->start;
                    line_data->token_count = (int16_t)(tok->tokens->count - tokens_at_start_of_line);
                    
                    line_count += 1;
                    Assert(line_count <= (int64_t)tok->line_data->capacity);

                    //
                    // new line data
                    //

                    line_data = tok->line_data->Push();
                    ZeroStruct(line_data);

                    line_data->range.start = tok->at - tok->start;
                    line_data->token_index = (int32_t)tok->tokens->count;
                    tokens_at_start_of_line = tok->tokens->count;
                }
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
        if (at_newline)
        {
            t.flags |= TokenFlag_FirstInLine;
        }
        at_newline = false;

        uint8_t *start = tok->at;

        uint8_t c = tok->at[0];
        tok->at += 1;

        switch (c)
        {
            default:
            {
parse_default:
                if (c == 'L' && Match(tok, '"'))       { goto parse_string; } 
                if (c == 'u' && Match(tok, '"'))       { goto parse_string; }
                if (c == 'U' && Match(tok, '"'))       { goto parse_string; }
                if (c == 'u' && Match(tok, "8\""_str)) { goto parse_string; }

                if (IsHeadUtf8Byte(c) || IsAlphabeticAscii(c) || c == '_')
                {
                    t.kind = Token_Identifier;
                    while (CharsLeft(tok) &&
                           (IsValidIdentifierAscii(Peek(tok)) || IsUtf8Byte(Peek(tok))))
                    {
                        tok->at += 1;
                    }
                }
                else if (IsNumericAscii(c))
                {
                    t.kind = Token_Number;
                    // TODO: Correct numeric literal rules
                    while (CharsLeft(tok) &&
                           (IsValidIdentifierAscii(Peek(tok)) ||
                            Peek(tok) == '.' ||
                            Peek(tok) == '\''))
                    {
                        tok->at += 1;
                    }
                }
                else
                {
                    t.kind = (TokenKind)c;
                }
            } break;

            case '_':
            {
                if (prev_token && prev_token->kind == Token_String)
                {
                    t.kind = Token_Operator;
                    while (IsValidIdentifierAscii(Peek(tok))) tok->at += 1;
                }
                else
                {
                    goto parse_default;
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
parse_string:
                t.kind = Token_String;
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

                        if (Peek(tok) == '\r') tok->at += 1;
                    }

                    if (Match(tok, '"'))
                    {
                        break;
                    }

                    tok->at += 1;
                }
            } break;

            case '\'':
            {
                if (Peek(tok) != '\'')
                {
                    if (Match(tok, '\\') ||
                        Peek(tok) != '\'')
                    {
                        tok->at++;
                        if (Match(tok, '\''))
                        {
                            t.kind = Token_CharacterLiteral;
                            break;
                        }
                    }
                }

                goto parse_default;
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

            case '<':
            {
                if (tok->in_preprocessor)
                {
                    t.kind = Token_String;
                    while (CharsLeft(tok))
                    {
                        // FIXME: SHOULD EMIT LINE DATA PROPERLY!!
                        // also deduplicate this code
                        if ((Peek(tok, 0) == '\r' && Peek(tok, 1) == '\n') ||
                            Peek(tok, 0) == '\n')
                        {
                            if (Peek(tok, -1) != '\\')
                            {
                                break;
                            }

                            if (Peek(tok) == '\r') tok->at += 1;
                        }

                        if (Match(tok, '>'))
                        {
                            break;
                        }

                        tok->at += 1;
                    }
                }
                else
                {
                    goto parse_default;
                }
            } break;

            case '*':
            {
                if (CharsLeft(tok) && Peek(tok) == '/')
                {
                    tok->at += 1;
                    if (tok->block_comment_count > 0)
                    {
                        if (language->allow_nested_block_comments)
                        {
                            tok->block_comment_count -= 1;
                        }
                        else
                        {
                            tok->block_comment_count = 0;
                        }
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

            case '(':
            {
                t.kind = Token_LeftParen;
                if (prev_token && prev_token->kind == Token_Identifier)
                {
                    prev_token->kind = Token_Function;
                }
            } break;
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

        t.pos    = tok->base + (int64_t)(start - tok->start);
        t.length = (int32_t)(end - start);

        prev_token = PushToken(tok, t);
    }

    (*tok->line_data)[line_count - 1].range.end = tok->end - tok->start;
}

function void
TokenizeBuffer(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    Tokenizer tok = {};
    tok.tokens    = &buffer->tokens;
    tok.line_data = &buffer->line_data;
    tok.language  = buffer->language;
    tok.start     = buffer->text;
    tok.end       = buffer->text + buffer->count;
    Tokenize(&tok);

    PlatformHighResTime end = platform->GetTime();
    double time = platform->SecondsElapsed(start, end);

    platform->DebugPrint("Tokenized in %fms\n", 1000.0*time);
}

function void
RetokenizeRange(Buffer *buffer, int64_t pos, int64_t delta)
{
    if (delta == 0) return;

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
    Tokenize(&tok);

    for (size_t i = max_token_index; i < buffer->tokens.count; i += 1)
    {
        Token *t = &buffer->tokens[i];
        t->pos += delta;
    }

    platform->DebugPrint("Retokenize\n");
    platform->DebugPrint("\tpos: %lld, delta: %lld\n", pos, delta);
    platform->DebugPrint("\tedit token index: %u\n", max_token_index);
}

