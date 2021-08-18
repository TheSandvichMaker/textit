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
                if (prev_t->kind == Token_Identifier && !(prev_t->flags & TokenFlag_IsPreprocessor))
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

function bool
ConsumeCppType(TagParser *parser)
{
    bool result = false;

    Token *rewind_point = PeekToken(parser);

    while (ConsumeToken(parser, Token_Keyword, "const"_str) ||
           ConsumeToken(parser, Token_Keyword, "volatile"_str));

    if (ConsumeToken(parser, Token_Identifier) ||
        ConsumeToken(parser, Token_Type))
    {
        for (;;)
        {
            if (!ConsumeToken(parser, Token_Operator, Token_Cpp_Namespace))
            {
                break;
            }
            if (!ConsumeToken(parser, Token_Identifier) &&
                !ConsumeToken(parser, Token_Type))
            {
                goto bail;
            }
        }
    }
    else
    {
        goto bail;
    }

    if (ConsumeToken(parser, '<'))
    {
        for (;;)
        {
            ConsumeCppType(parser);

            if (ConsumeToken(parser, '>'))
            {
                break;
            }

            if (!ConsumeToken(parser, ','))
            {
                goto bail;
            }
        }
    }

    while (ConsumeToken(parser, Token_Keyword, "const"_str)    ||
           ConsumeToken(parser, Token_Keyword, "volatile"_str) ||
           ConsumeToken(parser, '*')                           ||
           ConsumeToken(parser, '&'));

    result = true;

bail:
    if (!result)
    {
        Rewind(parser, rewind_point);
    }

    return result;
}

function void
ParseTagsCpp(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    FreeAllTags(buffer);

    TagParser parser_;
    TagParser *parser = &parser_;
    InitializeTagParser(parser, buffer);

    while (TokensLeft(parser))
    {
        ScopedMemory temp;

        Token *rewind_point = PeekToken(parser);

        SetFlags(parser, 0, TokenFlag_IsComment|TokenFlag_IsPreprocessor);
        TagSubKind match_kind = Tag_C_None;
        if      (ConsumeToken(parser, Token_Keyword, "struct"_str)) match_kind = Tag_C_Struct;
        else if (ConsumeToken(parser, Token_Keyword, "union"_str))  match_kind = Tag_C_Union;
        else if (ConsumeToken(parser, Token_Keyword, "enum"_str))   match_kind = Tag_C_Enum;
        else if (ConsumeToken(parser, Token_Keyword, "class"_str))  match_kind = Tag_Cpp_Class;
        if (match_kind)
        {
            if (Token *ident = ConsumeToken(parser, Token_Identifier))
            {
                Tag *tag = AddTag(buffer, ident);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = match_kind;
                if (ConsumeToken(parser, ':'))
                {
                    ConsumeToken(parser, Token_Identifier);
                }
                if (Token *opening_brace = ConsumeToken(parser, Token_LeftScope))
                {
                    tag->kind = Tag_Definition;
                    if (match_kind == Tag_C_Enum)
                    {
                        while (TokensLeft(parser) && !ConsumeToken(parser, Token_RightScope))
                        {
                            if (Token *enum_value = ConsumeToken(parser, Token_Identifier))
                            {
                                Tag *child_tag = AddTag(buffer, enum_value);
                                child_tag->parent   = tag;
                                child_tag->kind     = Tag_Declaration;
                                child_tag->sub_kind = Tag_C_EnumValue;
                            }
                            while (TokensLeft(parser) && !ConsumeToken(parser, ',') && !MatchToken(parser, Token_RightScope))
                            {
                                Advance(parser);
                            }
                        }
                    }
                }
            }
        }
        else if (ConsumeToken(parser, Token_Keyword, "typedef"_str))
        {
            ConsumeCppType(parser);
            if (Token *t = ConsumeToken(parser, Token_Identifier))
            {
                Tag *tag = AddTag(buffer, t);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = Tag_C_Typedef;
            }
        }
        else if (ConsumeCppType(parser)) // TODO: More robust function parsing
        {
            if (Token *t = ConsumeToken(parser, Token_Function))
            {
                Tag *tag = AddTag(buffer, t);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = Tag_C_Function;
                if (ConsumeBalancedPair(parser, Token_LeftParen) &&
                    ConsumeBalancedPair(parser, Token_LeftScope))
                {
                    tag->kind = Tag_Definition;
                }
            }
            else
            {
                Rewind(parser, rewind_point);
                Advance(parser);
            }
        }
        else if (SetFlags(parser, TokenFlag_IsPreprocessor, TokenFlag_IsComment), ConsumeToken(parser, Token_Preprocessor))
        {
            if (ConsumeToken(parser, Token_Identifier, "define"_str))
            {
                if (Token *t = ConsumeToken(parser, Token_Identifier))
                {
                    Tag *tag = AddTag(buffer, t);
                    tag->kind     = Tag_Definition;
                    tag->sub_kind = Tag_C_Macro;
                    if (ConsumeToken(parser, Token_LeftParen))
                    {
                        tag->related_token_kind = Token_Function;
                        tag->sub_kind           = Tag_C_FunctionMacro;
                    }
                }
            }
        }
        else
        {
            Advance(parser);
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

    AddTagSubKind(lang, Tag_C_Struct,        "struct"_str,         "text_type"_id);
    AddTagSubKind(lang, Tag_C_Union,         "union"_str,          "text_type"_id);
    AddTagSubKind(lang, Tag_C_Typedef,       "typedef"_str,        "text_type"_id);
    AddTagSubKind(lang, Tag_C_Enum,          "enum"_str,           "text_type"_id);
    AddTagSubKind(lang, Tag_C_EnumValue,     "enum_value"_str,     "text_literal"_id);
    AddTagSubKind(lang, Tag_C_Function,      "function"_str,       "text_function"_id);
    AddTagSubKind(lang, Tag_C_Macro,         "macro"_str,          "text_macro"_id);
    AddTagSubKind(lang, Tag_C_FunctionMacro, "function-macro"_str, "text_function_macro"_id);
    AddTagSubKind(lang, Tag_Cpp_Class,       "class"_str,          "text_type"_id);

    AddOperator(lang, "{"_str, Token_LeftScope);
    AddOperator(lang, "}"_str, Token_RightScope);
    AddOperator(lang, "("_str, Token_LeftParen);
    AddOperator(lang, ")"_str, Token_RightParen);
    AddOperator(lang, "\\"_str, Token_LineContinue);

    AddOperator(lang, "/*"_str, Token_OpenBlockComment);
    AddOperator(lang, "*/"_str, Token_CloseBlockComment);
    AddOperator(lang, "//"_str, Token_LineComment);

    AddOperator(lang, "::"_str, Token_Operator, Token_Cpp_Namespace);
    AddOperator(lang, "->"_str, Token_Operator, Token_C_Arrow);

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
    AddKeyword(lang, "template"_id, Token_Keyword);
    AddKeyword(lang, "typename"_id, Token_Keyword);

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
    AddKeyword(lang, "__int64"_id, Token_Type);
    AddKeyword(lang, "__m128"_id, Token_Type);
    AddKeyword(lang, "__m128i"_id, Token_Type);
    AddKeyword(lang, "__m128d"_id, Token_Type);
    AddKeyword(lang, "__m128h"_id, Token_Type);
    AddKeyword(lang, "__m256"_id, Token_Type);
    AddKeyword(lang, "__m256i"_id, Token_Type);
    AddKeyword(lang, "__m256d"_id, Token_Type);
    AddKeyword(lang, "__m256h"_id, Token_Type);
    AddKeyword(lang, "__m512"_id, Token_Type);
    AddKeyword(lang, "__m512i"_id, Token_Type);
    AddKeyword(lang, "__m512d"_id, Token_Type);
    AddKeyword(lang, "__m512h"_id, Token_Type);
    AddKeyword(lang, "__mmask8"_id, Token_Type);
    AddKeyword(lang, "__mmask16"_id, Token_Type);
    AddKeyword(lang, "__mmask32"_id, Token_Type);
}
END_REGISTER_LANGUAGE
