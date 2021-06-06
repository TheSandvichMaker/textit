static inline TokenBlock *
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

static inline void
PushToken(Tokenizer *tok, const Token &t)
{
    TokenList *list = tok->tokens;
    if (!list->first ||
        list->last->count >= ArrayCount(list->last->tokens))
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

static inline void
TokenizeBuffer(Buffer *buffer)
{
    Tokenizer tok_ = {};
    Tokenizer *tok = &tok_;

    tok->buffer = buffer;
    tok->at = buffer->text;
    tok->end = buffer->text + buffer->count;

    FreeTokens(buffer->prev_tokens);
    tok->tokens = buffer->prev_tokens;

    LanguageSpec *language = buffer->language;

    double total_matching_time = 0.0;
    int64_t tokens_matched = 0;

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

        PlatformHighResTime start_time = platform->GetTime();

        String string = MakeString((size_t)(end - start), start);
        if (t.kind == Token_Identifier)
        {
            TokenKind kind = (TokenKind)PointerToInt(StringMapFind(language->idents, string));
            if (kind)
            {
                t.kind = kind;
            }
        }

        PlatformHighResTime end_time = platform->GetTime();
        double time = platform->SecondsElapsed(start_time, end_time);

        total_matching_time += time;
        tokens_matched += 1;

        PushToken(tok, t);
    }

    platform->DebugPrint("Total matching time: %fms for %lld tokens (%fns/tok).\n",
                         1000.0*total_matching_time,
                         tokens_matched,
                         1000000000.0*total_matching_time / (double)tokens_matched);

    TokenList *tokens = (TokenList *)buffer->tokens;
    AtomicExchange((void *volatile*)&buffer->tokens, buffer->prev_tokens);
    buffer->prev_tokens = tokens;
}

static
PLATFORM_JOB(TokenizeBufferJob)
{
    Buffer *buffer = (Buffer *)userdata;
    buffer->tokenizing = true;
    TokenizeBuffer(buffer);
    buffer->tokenizing = false;
}

static inline LanguageSpec *
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
