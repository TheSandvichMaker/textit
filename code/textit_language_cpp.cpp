function void
TokenizeCpp(Tokenizer *tok)
{
    while (CharsLeft(tok))
    {
        if (ParseWhitespace(tok))
        {
            break;
        }

        Token t;
        BeginToken(tok, &t);

        Token *prev_t = tok->prev_token;

        if ((t.flags & TokenFlag_FirstInLine) &&
            (tok->user_state & TokenizeState_C_InPreprocessor) &&
            (prev_t->kind != Token_LineContinue))
        {
            tok->user_state &= ~TokenizeState_C_InPreprocessor;
            tok->state      &= ~TokenizeState_ImplicitStatementEndings;
        }

        uint8_t c = Advance(tok);
        if (c == '(')
        {
            int y = 0; (void)y;
        }
        switch (c)
        {
            default:
            {
parse_default:
                bool parse_string = false;
                char string_end_char = '"';
                if (c == 'L' && Match(tok, '"'))       { parse_string = true; } 
                if (c == 'u' && Match(tok, '"'))       { parse_string = true; }
                if (c == 'U' && Match(tok, '"'))       { parse_string = true; }
                if (c == 'u' && Match(tok, "8\""_str)) { parse_string = true; }
                if (tok->user_state & TokenizeState_C_InPreprocessor && Match(tok, '<'))
                {
                    parse_string = true;
                    string_end_char = '>';
                }

                if (parse_string)
                {
                    ParseString(tok, &t, string_end_char);
                    break;
                }

                Revert(tok, &t);
                ParseStandardToken(tok, &t);
            } break;

            case '_':
            {
                if (prev_t->kind == Token_String)
                {
                    t.kind = Token_Operator;
                    while (IsValidIdentifierAscii(Peek(tok))) Advance(tok);
                }
                else
                {
                    goto parse_default;
                }
            } break;

            case '#':
            {
                if (t.flags & TokenFlag_FirstInLine)
                {
                    t.kind = Token_Preprocessor;
                    tok->user_state |= TokenizeState_C_InPreprocessor;
                    tok->state      |= TokenizeState_ImplicitStatementEndings;
                }
            } break;

            case ':':
            {
                if ((prev_t->kind == Token_Identifier) &&
                    (prev_t->flags & TokenFlag_FirstInLine))
                {
                    prev_t->kind = Token_Label;
                }
                goto parse_default;
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

            case '(':
            {
                if (prev_t->kind == Token_Identifier)
                {
                    prev_t->kind = Token_Function;
                }
                goto parse_default;
            } break;
        }

        if (tok->user_state & TokenizeState_C_InPreprocessor)
        {
            t.flags |= TokenFlag_IsPreprocessor;
        }

        EndToken(tok, &t);
    }
}

function void
ParseTagsCpp(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    FreeAllTags(buffer);

    TokenIterator it = IterateTokens(buffer);
    while (IsValid(&it))
    {
        Token *t = Next(&it);
        if (!t) break;

        ScopedMemory temp;
        String text = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));

        if (AreEqual(text, "struct"_str) ||
            AreEqual(text, "enum"_str)   ||
            AreEqual(text, "class"_str)  ||
            AreEqual(text, "union"_str))
        {
            t = Next(&it);

            if (t->kind == Token_Identifier)
            {
                String name = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));
                Tag *tag = AddTag(buffer->tags, buffer, name);
                tag->kind   = Tag_Declaration;
                tag->pos    = t->pos;
                tag->length = t->length;
            }
        }

        if (AreEqual(text, "typedef"_str))
        {
            t = Next(&it);
            t = Next(&it);
            if (!t) break;

            if (t->kind == Token_Identifier)
            {
                String name = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));
                Tag *tag = AddTag(buffer->tags, buffer, name);
                tag->kind   = Tag_Declaration;
                tag->pos    = t->pos;
                tag->length = t->length;
            }
        }
    }

    PlatformHighResTime end = platform->GetTime();
    platform->DebugPrint("Tag parse time: %fms\n", 1000.0*platform->SecondsElapsed(start, end));
}

BEGIN_REGISTER_LANGUAGE("c++", lang)
{
    AssociateExtension(lang, "cpp"_str);
    AssociateExtension(lang, "hpp"_str);
    AssociateExtension(lang, "C"_str);
    AssociateExtension(lang, "cc"_str);
    AssociateExtension(lang, "h"_str);

    lang->Tokenize  = TokenizeCpp;
    lang->ParseTags = ParseTagsCpp;

    AddOperator(lang, "{"_str, Token_LeftScope);
    AddOperator(lang, "}"_str, Token_RightScope);
    AddOperator(lang, "("_str, Token_LeftParen);
    AddOperator(lang, ")"_str, Token_RightParen);
    AddOperator(lang, "\\"_str, Token_LineContinue);

    AddOperator(lang, "/*"_str, Token_OpenBlockComment);
    AddOperator(lang, "*/"_str, Token_CloseBlockComment);
    AddOperator(lang, "//"_str, Token_LineComment);

    AddKeyword(lang, "static"_id, Token_Keyword);
    AddKeyword(lang, "inline"_id, Token_Keyword);
    AddKeyword(lang, "operator"_id, Token_Keyword);
    AddKeyword(lang, "volatile"_id, Token_Keyword);
    AddKeyword(lang, "extern"_id, Token_Keyword);
    AddKeyword(lang, "const"_id, Token_Keyword);
    AddKeyword(lang, "struct"_id, Token_Keyword);
    AddKeyword(lang, "union"_id, Token_Keyword);
    AddKeyword(lang, "enum"_id, Token_Keyword);
    AddKeyword(lang, "class"_id, Token_Keyword);
    AddKeyword(lang, "public"_id, Token_Keyword);
    AddKeyword(lang, "private"_id, Token_Keyword);
    AddKeyword(lang, "thread_local"_id, Token_Keyword);
    AddKeyword(lang, "final"_id, Token_Keyword);
    AddKeyword(lang, "constexpr"_id, Token_Keyword);
    AddKeyword(lang, "consteval"_id, Token_Keyword);
    AddKeyword(lang, "typedef"_id, Token_Keyword);
    AddKeyword(lang, "using"_id, Token_Keyword);

    AddKeyword(lang, "case"_id, Token_FlowControl);
    AddKeyword(lang, "default"_id, Token_FlowControl);
    AddKeyword(lang, "continue"_id, Token_FlowControl);
    AddKeyword(lang, "break"_id, Token_FlowControl);
    AddKeyword(lang, "if"_id, Token_FlowControl);
    AddKeyword(lang, "else"_id, Token_FlowControl);
    AddKeyword(lang, "for"_id, Token_FlowControl);
    AddKeyword(lang, "switch"_id, Token_FlowControl);
    AddKeyword(lang, "while"_id, Token_FlowControl);
    AddKeyword(lang, "do"_id, Token_FlowControl);
    AddKeyword(lang, "goto"_id, Token_FlowControl);
    AddKeyword(lang, "return"_id, Token_FlowControl);

    AddKeyword(lang, "true"_id, Token_Literal);
    AddKeyword(lang, "false"_id, Token_Literal);
    AddKeyword(lang, "nullptr"_id, Token_Literal);
    AddKeyword(lang, "NULL"_id, Token_Literal);

    AddKeyword(lang, "unsigned"_id, Token_Type);
    AddKeyword(lang, "signed"_id, Token_Type);
    AddKeyword(lang, "register"_id, Token_Type);
    AddKeyword(lang, "bool"_id, Token_Type); 
    AddKeyword(lang, "void"_id, Token_Type); 
    AddKeyword(lang, "char"_id, Token_Type); 
    AddKeyword(lang, "char_t"_id, Token_Type); 
    AddKeyword(lang, "char16_t"_id, Token_Type); 
    AddKeyword(lang, "char32_t"_id, Token_Type); 
    AddKeyword(lang, "wchar_t"_id, Token_Type); 
    AddKeyword(lang, "short"_id, Token_Type); 
    AddKeyword(lang, "int"_id, Token_Type); 
    AddKeyword(lang, "long"_id, Token_Type); 
    AddKeyword(lang, "float"_id, Token_Type); 
    AddKeyword(lang, "double"_id, Token_Type);
    AddKeyword(lang, "int8_t"_id, Token_Type); 
    AddKeyword(lang, "int16_t"_id, Token_Type); 
    AddKeyword(lang, "int32_t"_id, Token_Type); 
    AddKeyword(lang, "int64_t"_id, Token_Type);
    AddKeyword(lang, "uint8_t"_id, Token_Type); 
    AddKeyword(lang, "uint16_t"_id, Token_Type); 
    AddKeyword(lang, "uint32_t"_id, Token_Type); 
    AddKeyword(lang, "uint64_t"_id, Token_Type);
    AddKeyword(lang, "intptr_t"_id, Token_Type);
    AddKeyword(lang, "uintptr_t"_id, Token_Type);
    AddKeyword(lang, "size_t"_id, Token_Type);
    AddKeyword(lang, "ssize_t"_id, Token_Type);
    AddKeyword(lang, "ptrdiff_t"_id, Token_Type);
}
END_REGISTER_LANGUAGE
