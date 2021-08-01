function LineData *
AllocateLineData(void)
{
    LineData *result = PushStruct(&editor->transient_arena, LineData);
    return result;
}

function Token *
PushToken(Tokenizer *tok, const Token &t)
{
    return tok->buffer->tokens.Push(t);
}

function int64_t
AtPos(Tokenizer *tok)
{
    return (int64_t)(tok->at - tok->buffer->text);
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

function void
TokenizeBuffer(Buffer *buffer)
{
    Tokenizer tok_ = {};
    Tokenizer *tok = &tok_;

    tok->buffer = buffer;
    tok->start = buffer->text;
    tok->at = tok->start;
    tok->end = buffer->text + buffer->count;

    buffer->tokens.Clear();
    buffer->tokens.EnsureSpace(1); // TODO: jank alert? forcing the allocation so that line_data->tokens below will not be null
    buffer->line_data.Clear();

    LanguageSpec *language = buffer->language;

    PlatformHighResTime start_time = platform->GetTime();

    int line_count = 1;
    int tokens_at_start_of_line = 0;

    bool at_newline = true;

    LineData *line_data = buffer->line_data.Push();
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
                    line_data->token_count = (int16_t)(buffer->tokens.count - tokens_at_start_of_line);
                    
                    line_count += 1;
                    Assert(line_count <= 1000000);

                    //
                    // new line data
                    //

                    line_data = buffer->line_data.Push();
                    ZeroStruct(line_data);

                    line_data->range.start = tok->at - tok->start;
                    line_data->token_index = (int32_t)buffer->tokens.count;
                    tokens_at_start_of_line = buffer->tokens.count;
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
parse_operator:
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
                    Match(tok, '\\');
                    if (Peek(tok) != '\'')
                    {
                        tok->at++;
                        if (Match(tok, '\''))
                        {
                            t.kind = Token_CharacterLiteral;
                            break;
                        }
                    }
                }

                goto parse_operator;
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
                    goto parse_operator;
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

            case '=':
            {
                if (Match(tok, '='))
                {
                    t.kind = Token_Equals;
                }
                else
                {
                    t.kind = Token_Assignment;
                }
            } break;

            case '!':
            {
                if (Match(tok, '='))
                {
                    t.kind = Token_NotEquals;
                }
                else
                {
                    t.kind = (TokenKind)'!';
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
        t.length = (int32_t)(end - start);

#if 0
        String string = MakeString((size_t)(end - start), start);
        if (t.kind == Token_Identifier)
        {
            TokenKind kind = (TokenKind)PointerToInt(StringMapFind(language->idents, string));
            if (kind)
            {
                t.kind = kind;
            }
        }
#endif

        prev_token = PushToken(tok, t);
    }

    buffer->line_data[line_count - 1].range.end = buffer->count;

    PlatformHighResTime end_time = platform->GetTime();

#if 1
    double total_time = platform->SecondsElapsed(start_time, end_time);

    platform->DebugPrint("Total time: %fms for %lld tokens, %lld characters (%fns/tok, %fns/char).\n",
                         1000.0*total_time,
                         buffer->tokens.count,
                         buffer->count,
                         1'000'000'000.0*total_time / (double)buffer->tokens.count,
                         1'000'000'000.0*total_time / (double)buffer->count);
    platform->DebugPrint("%f megatokens/s, %fmb/s\n", 
                         (double)buffer->tokens.count / (1'000'000.0*total_time),
                         (double)buffer->count / (1'000'000.0*total_time));
#endif
}

function LanguageSpec *
PushCppLanguageSpec(Arena *arena)
{
    LanguageSpec *result = PushStruct(arena, LanguageSpec);
    result->idents = PushStringMap(arena, 128);

    StringMapInsert(result->idents, "static"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "static"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "inline"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "operator"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "volatile"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "unsigned"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "signed"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "register"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "extern"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "const"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "struct"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "enum"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "class"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "public"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "private"_str, IntToPointer(Token_Keyword));
    StringMapInsert(result->idents, "return"_str, IntToPointer(Token_Keyword));

    StringMapInsert(result->idents, "case"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "continue"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "break"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "if"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "else"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "for"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "switch"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "while"_str, IntToPointer(Token_FlowControl));
    StringMapInsert(result->idents, "do"_str, IntToPointer(Token_FlowControl));

    StringMapInsert(result->idents, "true"_str, IntToPointer(Token_Literal));
    StringMapInsert(result->idents, "false"_str, IntToPointer(Token_Literal));
    StringMapInsert(result->idents, "nullptr"_str, IntToPointer(Token_Literal));
    StringMapInsert(result->idents, "NULL"_str, IntToPointer(Token_Literal));

    StringMapInsert(result->idents, "void"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "char"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "short"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "int"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "long"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "float"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "double"_str, IntToPointer(Token_Type));
    StringMapInsert(result->idents, "int8_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "int16_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "int32_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "int64_t"_str, IntToPointer(Token_Type));
    StringMapInsert(result->idents, "uint8_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "uint16_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "uint32_t"_str, IntToPointer(Token_Type)); 
    StringMapInsert(result->idents, "uint64_t"_str, IntToPointer(Token_Type));

    return result;
}
