function LineData *
AllocateLineData(void)
{
    LineData *result = PushStruct(&editor_state->transient_arena, LineData);
    return result;
}

function TokenBlock *
AllocateTokenBlock(void)
{
    if (!editor_state->first_free_token_block)
    {
        editor_state->first_free_token_block = PushStructNoClear(&editor_state->transient_arena, TokenBlock);
        editor_state->first_free_token_block->next = nullptr;
        editor_state->first_free_token_block->prev = nullptr;
    }
    TokenBlock *result = editor_state->first_free_token_block;
    editor_state->first_free_token_block = result->next;

    ZeroStruct(result);
    result->min_pos = INT64_MAX;
    result->max_pos = INT64_MIN;
    return result;
}

function Token *
PushToken(Tokenizer *tok, const Token &t)
{
    TokenList *list = tok->tokens;
    if (!list->first ||
        list->last->count >= (int64_t)ArrayCount(list->last->tokens))
    {
        TokenBlock *block = AllocateTokenBlock();
        if (list->first)
        {
            block->prev = list->last;
            list->last = list->last->next = block;
        }
        else
        {
            list->first = list->last = block;
        }
    }
    TokenBlock *block = list->last;

    if (t.pos < block->min_pos) block->min_pos = t.pos;
    if (t.pos + t.length > block->max_pos) block->max_pos = t.pos + t.length;

    block->tokens[block->count++] = t;
    tok->token_count += 1;

    return &block->tokens[block->count - 1];
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

function void
FreeTokens(TokenList *list)
{
    if (list->last)
    {
        list->last->next = editor_state->first_free_token_block;
    }
    editor_state->first_free_token_block = list->first;

    list->first = nullptr;
    list->last  = nullptr;
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

    FreeTokens(&buffer->tokens);
    tok->tokens = &buffer->tokens;

    LanguageSpec *language = buffer->language;

    int64_t token_count = 0;
    PlatformHighResTime start_time = platform->GetTime();

    int line_count = 1;
    int tokens_at_start_of_line = 0;

    bool at_newline = true;

    LineData *line_data = &buffer->line_data[0];
    ZeroStruct(line_data);

    line_data->tokens = tok->tokens->first;

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

                    line_data->whitespace_pos = tok->at - tok->start - 1;

                    if ((tok->at[-1] == '\r') && (tok->at[0] == '\n'))
                    {
                        tok->at += 1;
                    }

                    line_data->range.end = tok->at - tok->start;
                    line_data->token_count = tok->token_count - tokens_at_start_of_line;
                    
                    line_count += 1;
                    Assert(line_count <= 1000000);

                    //
                    // new line data
                    //

                    line_data = &buffer->line_data[line_count - 1];
                    ZeroStruct(line_data);

                    line_data->range.start = tok->at - tok->start;
                    line_data->tokens = tok->tokens->last;
                    line_data->token_offset = line_data->tokens->count;
                    tokens_at_start_of_line = tok->token_count;
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

            case ';': { t.kind = Token_StatementEnd; } break;
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

        token_count += 1;

        prev_token = PushToken(tok, t);
    }

    buffer->line_data[line_count - 1].range.end = buffer->count;
    buffer->line_count = line_count;

    PlatformHighResTime end_time = platform->GetTime();

#if 0
    double total_time = platform->SecondsElapsed(start_time, end_time);

    platform->DebugPrint("Total time: %fms for %lld tokens, %lld characters (%fns/tok, %fns/char).\n",
                         1000.0*total_time,
                         token_count,
                         buffer->count,
                         1'000'000'000.0*total_time / (double)token_count,
                         1'000'000'000.0*total_time / (double)buffer->count);
    platform->DebugPrint("%f megatokens/s, %fmb/s\n", 
                         (double)token_count / (1'000'000.0*total_time),
                         (double)buffer->count / (1'000'000.0*total_time));
#endif
}

#if 0
static
PLATFORM_JOB(TokenizeBufferJob)
{
    Buffer *buffer = (Buffer *)userdata;
    buffer->tokenizing = true;
    TokenizeBuffer(buffer);
    buffer->tokenizing = false;
}
#endif

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
