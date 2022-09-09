// tag:FuckedLineStateStuff: between in_preprocessor, TokenizeState_C_InPreprocessor and LineTokenizeState_Preprocessor I think we have
// enough ways of specifying this shit. Same for in_string etc. And TokenizeState is junk. LineTokenizeState is relevant.

function int
DecodeDigit(uint8_t c, int base = 10)
{
    int result = -1;

    c = ToLowerAscii(c);

    if (c >= '0' && c <= '9')
    {
        int value = c - '0';
        if (value >= 0 && value < base)
        {
            result = value;
        }
    }
    else if (c >= 'a' && c <= 'f')
    {
        int value = 10 + c - 'a';
        if (value >= 10 && value < base)
        {
            result = value;
        }
    }

    return result;
}

function bool
ParseNumber(Tokenizer *tok, int base = 10, int max = INT_MAX)
{
    bool result = false;

    if (DecodeDigit(Peek(tok), base) > 0)
    {
        result = true;

        Advance(tok);
        max -= 1;
        while (max > 0 && CharsLeft(tok) && DecodeDigit(Peek(tok), base) != -1)
        {
            Advance(tok);
            max -= 1;
        }
    }

    return true;
}

function bool
ParseEscapeSequence(Tokenizer *tok, Token *t)
{
    bool result = false;

    switch (Peek(tok))
    {
        case 'a':
        case 'b':
        case 'e':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '\\':
        case '\'':
        case '"':
        case '\?':
        {
            Advance(tok);
            result = true;
        } break;

        case 'x':
        {
            Advance(tok);
            result = ParseNumber(tok, 16, 2);
        } break;

        case 'u':
        {
            Advance(tok);
            result = ParseNumber(tok, 16, 4);
        } break;

        case 'U':
        {
            Advance(tok);
            result = ParseNumber(tok, 16, 8);
        } break;

        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
        case '8':
        {
            result = ParseNumber(tok, 8, 3);
        } break;
    }

    if (result)
    {
        t->kind     = Token_Custom;
        t->sub_kind = Token_C_StringSpecial;
    }

    return result;
}

function bool
ParsePrintfFormatter(Tokenizer *tok, Token *t)
{
    bool result = false;

    // flags
    Match(tok, '-');
    Match(tok, '+');
    Match(tok, ' ');
    Match(tok, '#');
    Match(tok, '0');

    // width
    if (!Match(tok, '*'))
    {
        ParseNumber(tok);
    }

    // precision
    if (Match(tok, '.'))
    {
        if (!Match(tok, '*'))
        {
            ParseNumber(tok);
        }
    }

    // length
    switch (Peek(tok))
    {
        case 'h':
        case 'l':
        case 'j':
        case 'z':
        case 't':
        case 'L':
        {
            Advance(tok);
            switch (Peek(tok))
            {
                case 'h':
                case 'l':
                {
                    Advance(tok);
                } break;
            }
        } break;
    }

    // specifier
    switch (Peek(tok))
    {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
        case 'c':
        case 's':
        case 'p':
        case 'n':
        case '%':
        {
            Advance(tok);

            result = true;
            t->kind     = Token_Custom;
            t->sub_kind = Token_C_StringSpecial;
        } break;
    }

    return result;
}

function void
TokenizeCpp(Tokenizer *tok, Token *t)
{
    TokenizerCpp *tok_cpp = (TokenizerCpp *)tok->userdata;

    Token *prev_t = tok->prev_token;

    uint8_t c = Advance(tok);
    switch (c)
    {
        default:
        {
parse_default:
            bool parse_string = false;
            if (!tok->in_string)
            {
                if (c == 'L' && Match(tok, '"'))       { tok_cpp->string_end_char = '"'; parse_string = true; } 
                if (c == 'u' && Match(tok, '"'))       { tok_cpp->string_end_char = '"'; parse_string = true; }
                if (c == 'U' && Match(tok, '"'))       { tok_cpp->string_end_char = '"'; parse_string = true; }
                if (c == 'u' && Match(tok, "8\""_str)) { tok_cpp->string_end_char = '"'; parse_string = true; }
                if (HasFlag(tok->user_state, TokenizeState_C_InPreprocessor) && Peek(tok) == '<')
                {
                    ScopedMemory temp;
                    String prev_token_string = PushTokenString(temp, tok->buffer, prev_t);
                    if (AreEqual(prev_token_string, "include"_str))
                    {
                        parse_string = true;
                        tok_cpp->string_end_char = '>';
                    }
                }

                if (parse_string)
                {
                    t->kind = Token_StartString;
                    break;
                }
            }

            Revert(tok, t);
            ParseStandardToken(tok, t);
        } break;

        case '%':
        {
            if (!tok->in_string) goto parse_default;
            if (!ParsePrintfFormatter(tok, t)) goto parse_default;
        } break;

        case '\\':
        {
            if (!tok->in_string) goto parse_default;
            if (!ParseEscapeSequence(tok, t)) goto parse_default;
        } break;

        case '>':
        {
            if (tok->in_string && tok_cpp->string_end_char == '>')
            {
                t->kind = Token_EndString;
            }
            else
            {
                goto parse_default;
            }
        } break;

        case '"':
        {
            if (Peek(tok, -1) != '\\')
            {
                if (tok->in_string && tok_cpp->string_end_char == '"')
                {
                    t->kind = Token_EndString;
                }
                else if (!tok->in_string)
                {
                    t->kind = Token_StartString;
                    tok_cpp->string_end_char = '"';
                }
                else
                {
                    goto parse_default;
                }
            }
        } break;

        case '_':
        {
            if (prev_t->kind == Token_EndString)
            {
                t->kind = Token_Operator;
                while (IsValidIdentifierAscii(Peek(tok))) Advance(tok);
            }
            else
            {
                goto parse_default;
            }
        } break;

        case '#':
        {
            if (t->flags & TokenFlag_FirstInLine)
            {
                t->kind = Token_Preprocessor;
                // FuckedLineStateStuff
                tok->in_preprocessor = true;
                tok->user_state |= TokenizeState_C_InPreprocessor;
                tok->state      |= TokenizeState_ImplicitStatementEndings;
            }
            else
            {
                goto parse_default;
            }
        } break;

        case ':':
        {
            if ((Peek(tok) != ':') &&
                (prev_t->kind == Token_Identifier) &&
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
                        t->kind = Token_CharacterLiteral;
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

    // FuckedLineStateStuff
    if (tok->user_state & TokenizeState_C_InPreprocessor)
    {
        t->flags |= TokenFlag_IsPreprocessor;
    }
}

function bool
ConsumeCppType(TagParser *parser)
{
    bool result = false;

    TokenLocator rewind_point = GetLocator(parser);

    ConsumeToken(parser, Token_Keyword, "struct"_str); // account for forward declarations

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
	
	if (ConsumeToken(parser, '['))
	{
		for (;;)
		{
			if (!TokensLeft(parser)) goto bail;
			if (ConsumeToken(parser, ']'))
			{
				break;
			}
			else
			{
				Advance(parser);
			}
		}
	}

    result = true;

bail:
    if (!result)
    {
        Rewind(parser, rewind_point);
    }

    return result;
}

function bool
ConsumeCppIf0(TagParser *parser)
{
    TokenLocator rewind = GetLocator(parser);
    if (ConsumeToken(parser, Token_Preprocessor) &&
        ConsumeToken(parser, Token_FlowControl, "if"_str) &&
        (ConsumeToken(parser, Token_Number, "0"_str) ||
         ConsumeToken(parser, Token_Literal, "false"_str)))
    {
        return true;
    }
    Rewind(parser, rewind);
    return false;
}

function void
SkipComments(TagParser *parser)
{
    while (parser->it.token.flags & TokenFlag_IsComment)
    {
        Advance(parser);
    }
}

function void
ParseTagsCpp(Buffer *buffer)
{
    FreeAllTags(buffer);

    TagParser parser_;
    TagParser *parser = &parser_;
    InitializeTagParser(parser, buffer);
	
	// NestHelper scope_nest = {};

    while (TokensLeft(parser))
    {
        ScopedMemory temp;
		
		// Token peek_t = PeekToken(parser);
		// if (IsInNest(&scope_nest, peek_t.kind, Direction_Forward))
		// {
		// 	Advance(parser);
		// 	continue;
		// }

        SetFlags(parser, 0, TokenFlag_IsComment|TokenFlag_IsPreprocessor);
        TagSubKind match_kind = Tag_C_None;
        if      (ConsumeToken(parser, Token_Keyword, "struct"_str)) match_kind = Tag_C_Struct;
        else if (ConsumeToken(parser, Token_Keyword, "union"_str))  match_kind = Tag_C_Union;
        else if (ConsumeToken(parser, Token_Keyword, "enum"_str))   match_kind = Tag_C_Enum;
        else if (ConsumeToken(parser, Token_Keyword, "class"_str))  match_kind = Tag_Cpp_Class;
        if (match_kind)
        {
            if (Token ident = ConsumeToken(parser, Token_Identifier))
            {
                Tag *tag = AddTag(buffer, &ident);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = match_kind;
                if (ConsumeToken(parser, ':'))
                {
                    ConsumeToken(parser, Token_Identifier);
                }
                if (Token opening_brace = ConsumeToken(parser, Token_LeftScope))
                {
                    tag->kind = Tag_Definition;
                    if (match_kind == Tag_C_Enum)
                    {
                        while (TokensLeft(parser) && !ConsumeToken(parser, Token_RightScope))
                        {
                            SkipComments(parser); // jank alert: you'd expect comments to be automatically skipped when matching here, really
                                                  // but that's not how I architected the parser
                            if (Token enum_value = ConsumeToken(parser, Token_Identifier))
                            {
                                Tag *child_tag = AddTag(buffer, &enum_value);
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
                    else
                    {
                        ConsumeUpToAndIncluding(parser, Token_RightScope);
                    }
                }
            }
        }
        else if (ConsumeToken(parser, Token_Keyword, "typedef"_str))
        {
            // TODO: to handle C style struct typedefs this should be more clever.
            // ConsumeCppType should be something which can actually parse struct
            // declarations and emit the tag for it. Aka turn this more into a 
            // proper recursive descent parser.
            TokenLocator rewind = GetLocator(parser);

            ConsumeCppType(parser);
            if (Token t = ConsumeToken(parser, Token_Identifier))
            {
                Tag *tag = AddTag(buffer, &t);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = Tag_C_Typedef;
            }
            else
            {
                Rewind(parser, rewind);
            }
        }
        else if (ConsumeCppType(parser)) // TODO: More robust function parsing
        {
			Token t = {};
            if (t = ConsumeToken(parser, Token_Function))
            {
                Tag *tag = AddTag(buffer, &t);
                tag->kind     = Tag_Declaration;
                tag->sub_kind = Tag_C_Function;
                if (ConsumeBalancedPair(parser, Token_LeftParen) &&
                    ConsumeBalancedPair(parser, Token_LeftScope))
                {
                    tag->kind = Tag_Definition;
                }
            }
			/*
			else if (t = ConsumeToken(parser, Token_Identifier))
			{
                if (t = ConsumeToken(parser, ';'))
                {
                    Tag *tag = AddTag(buffer, &t);
                    tag->kind     = Tag_Declaration;
                    tag->sub_kind = Tag_C_Global;
                }
                else if (t = ConsumeToken(parser, '='))
                {
                    Tag *tag = AddTag(buffer, &t);
                    tag->kind     = Tag_Declaration;
                    tag->sub_kind = Tag_C_Global;
                    if (t.kind == '=') ConsumeUpToAndIncluding(parser, ';');
                }
			}
			*/
        }
        else if (SetFlags(parser, TokenFlag_IsPreprocessor, TokenFlag_IsComment), ConsumeToken(parser, Token_Preprocessor))
        {
            if (ConsumeToken(parser, Token_Identifier, "define"_str))
            {
                if (Token t = ConsumeToken(parser, Token_Identifier))
                {
                    Tag *tag = AddTag(buffer, &t);
                    tag->kind     = Tag_Definition;
                    tag->sub_kind = Tag_C_Macro;
                    if (Token left_paren = ConsumeToken(parser, Token_LeftParen))
                    {
                        // make sure there's no whitespace inbetween (TODO: I could look for a whitespace token if I provided that functionality)
                        if (left_paren.pos == t.pos + t.length)
                        {
                            tag->related_token_kind = Token_Function;
                            tag->sub_kind           = Tag_C_FunctionMacro;
                        }
                    }
                }
            }
        }
        else
        {
            ParseBuiltinTag(parser);
        }
    }
}

function CustomAutocompleteResult
CustomAutocompleteCpp(Arena *arena, Tag *tag, String text)
{
    CustomAutocompleteResult result = { text, (int64_t)text.size };

    if (tag->sub_kind == Tag_C_Function ||
        tag->sub_kind == Tag_C_FunctionMacro)
    {
        result.text = PushStringF(arena, "%.*s(", StringExpand(text));
        result.pos  = (int64_t)text.size + 1;
    }
    else
    {
        result.text = PushStringF(arena, "%.*s ", StringExpand(text));
        result.pos  = (int64_t)text.size + 1;
    }

    return result;
}

BEGIN_REGISTER_LANGUAGE("c++", lang)
{
    AssociateExtension(lang, "cpp"_str);
    AssociateExtension(lang, "hpp"_str);
    AssociateExtension(lang, "C"_str);
    AssociateExtension(lang, "cc"_str);
    AssociateExtension(lang, "h"_str);
	
	// for the time being, parse C as C++ as well
	AssociateExtension(lang, "c"_str);
	
    lang->tokenize_userdata_size = sizeof(TokenizerCpp);
    lang->Tokenize  = TokenizeCpp;
    lang->ParseTags = ParseTagsCpp;
    lang->CustomAutocomplete = CustomAutocompleteCpp;

    AddTokenSubKind(lang, Token_C_StringSpecial, "text_string_special"_id);

    AddTagSubKind(lang, Tag_C_Struct,        "struct"_str,         "text_type"_id);
    AddTagSubKind(lang, Tag_C_Union,         "union"_str,          "text_type"_id);
    AddTagSubKind(lang, Tag_C_Typedef,       "typedef"_str,        "text_type"_id);
    AddTagSubKind(lang, Tag_C_Enum,          "enum"_str,           "text_type"_id);
    AddTagSubKind(lang, Tag_C_EnumValue,     "enum_value"_str,     "text_literal"_id);
    AddTagSubKind(lang, Tag_C_Function,      "function"_str,       "text_function"_id);
    AddTagSubKind(lang, Tag_C_Macro,         "macro"_str,          "text_macro"_id);
    AddTagSubKind(lang, Tag_C_FunctionMacro, "function-macro"_str, "text_function_macro"_id);
    AddTagSubKind(lang, Tag_Cpp_Class,       "class"_str,          "text_type"_id);
	AddTagSubKind(lang, Tag_C_Global,        "global-variable"_str, "text_global"_id);

    AddOperator(lang, "{"_str, Token_LeftScope);
    AddOperator(lang, "}"_str, Token_RightScope);
    AddOperator(lang, "("_str, Token_LeftParen);
    AddOperator(lang, ")"_str, Token_RightParen);
    AddOperator(lang, "\\"_str, Token_LineContinue);

    AddOperator(lang, "/*"_str, Token_OpenBlockComment);
    AddOperator(lang, "*/"_str, Token_CloseBlockComment);
    AddOperator(lang, "//"_str, Token_LineComment);

    AddOperator(lang, "\""_str, Token_String);

    // I don't remember why I added these initially, but post-hoc I have justified it as
    // being for avoiding the first : in :: being used for labels, and the > in -> for
    // matching template nonsense
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
    AddKeyword(lang, "namespace"_id, Token_Keyword);
    AddKeyword(lang, "auto"_id, Token_Keyword);

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

    AddKeyword(lang, "true"_id, Token_Literal);
    AddKeyword(lang, "false"_id, Token_Literal);

    AddKeyword(lang, "nullptr"_id, Token_Literal);
    AddKeyword(lang, "NULL"_id, Token_Literal);

    AddKeyword(lang, "CHAR_BIT"_id, Token_Literal); 
    AddKeyword(lang, "MB_LEN_MAX"_id, Token_Literal); 
    AddKeyword(lang, "MB_CUR_MAX"_id, Token_Literal);
    
    AddKeyword(lang, "UCHAR_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_MAX"_id, Token_Literal); 
    AddKeyword(lang, "ULONG_MAX"_id, Token_Literal); 
    AddKeyword(lang, "USHRT_MAX"_id, Token_Literal);
    
    AddKeyword(lang, "CHAR_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_MIN"_id, Token_Literal); 
    AddKeyword(lang, "LONG_MIN"_id, Token_Literal); 
    AddKeyword(lang, "SHRT_MIN"_id, Token_Literal);
    
    AddKeyword(lang, "CHAR_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_MAX"_id, Token_Literal); 
    AddKeyword(lang, "LONG_MAX"_id, Token_Literal); 
    AddKeyword(lang, "SHRT_MAX"_id, Token_Literal);
    
    AddKeyword(lang, "SCHAR_MIN"_id, Token_Literal); 
    AddKeyword(lang, "SINT_MIN"_id, Token_Literal); 
    AddKeyword(lang, "SLONG_MIN"_id, Token_Literal); 
    AddKeyword(lang, "SSHRT_MIN"_id, Token_Literal);
    
    AddKeyword(lang, "SCHAR_MAX"_id, Token_Literal); 
    AddKeyword(lang, "SINT_MAX"_id, Token_Literal); 
    AddKeyword(lang, "SLONG_MAX"_id, Token_Literal); 
    AddKeyword(lang, "SSHRT_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "__func__"_id, Token_Literal); 
    AddKeyword(lang, "__VA_ARGS__"_id, Token_Literal);
      
    AddKeyword(lang, "LLONG_MIN"_id, Token_Literal); 
    AddKeyword(lang, "LLONG_MAX"_id, Token_Literal); 
    AddKeyword(lang, "ULLONG_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "INT8_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT16_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT32_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT64_MIN"_id, Token_Literal);
      
    AddKeyword(lang, "INT8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "UINT8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "INT_LEAST8_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST16_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST32_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST64_MIN"_id, Token_Literal);
      
    AddKeyword(lang, "INT_LEAST8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_LEAST64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "UINT_LEAST8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_LEAST16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_LEAST32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_LEAST64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "INT_FAST8_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST16_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST32_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST64_MIN"_id, Token_Literal);
      
    AddKeyword(lang, "INT_FAST8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "INT_FAST64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "UINT_FAST8_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_FAST16_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_FAST32_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINT_FAST64_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "INTPTR_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INTPTR_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINTPTR_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "INTMAX_MIN"_id, Token_Literal); 
    AddKeyword(lang, "INTMAX_MAX"_id, Token_Literal); 
    AddKeyword(lang, "UINTMAX_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "PTRDIFF_MIN"_id, Token_Literal); 
    AddKeyword(lang, "PTRDIFF_MAX"_id, Token_Literal); 
    AddKeyword(lang, "SIG_ATOMIC_MIN"_id, Token_Literal); 
    AddKeyword(lang, "SIG_ATOMIC_MAX"_id, Token_Literal);
      
    AddKeyword(lang, "SIZE_MAX"_id, Token_Literal); 
    AddKeyword(lang, "WCHAR_MIN"_id, Token_Literal); 
    AddKeyword(lang, "WCHAR_MAX"_id, Token_Literal); 
    AddKeyword(lang, "WINT_MIN"_id, Token_Literal); 
    AddKeyword(lang, "WINT_MAX"_id, Token_Literal);
    
    AddKeyword(lang, "FLT_RADIX"_id, Token_Literal); 
    AddKeyword(lang, "FLT_ROUNDS"_id, Token_Literal); 
    AddKeyword(lang, "FLT_DIG"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MANT_DIG"_id, Token_Literal); 
    AddKeyword(lang, "FLT_EPSILON"_id, Token_Literal); 
    AddKeyword(lang, "DBL_DIG"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MANT_DIG"_id, Token_Literal); 
    AddKeyword(lang, "DBL_EPSILON"_id, Token_Literal);
    
    AddKeyword(lang, "LDBL_DIG"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MANT_DIG"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_EPSILON"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MIN"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MAX"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MIN_EXP"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MAX_EXP"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MIN_10_EXP"_id, Token_Literal); 
    AddKeyword(lang, "FLT_MAX_10_EXP"_id, Token_Literal);
    
    AddKeyword(lang, "DBL_MIN"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MAX"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MIN_EXP"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MAX_EXP"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MIN_10_EXP"_id, Token_Literal); 
    AddKeyword(lang, "DBL_MAX_10_EXP"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MIN"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MAX"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MIN_EXP"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MAX_EXP"_id, Token_Literal);
    
    AddKeyword(lang, "LDBL_MIN_10_EXP"_id, Token_Literal); 
    AddKeyword(lang, "LDBL_MAX_10_EXP"_id, Token_Literal); 
    AddKeyword(lang, "HUGE_VAL"_id, Token_Literal); 
    AddKeyword(lang, "CLOCKS_PER_SEC"_id, Token_Literal); 
    AddKeyword(lang, "NULL"_id, Token_Literal); 
    AddKeyword(lang, "LC_ALL"_id, Token_Literal); 
    AddKeyword(lang, "LC_COLLATE"_id, Token_Literal); 
    AddKeyword(lang, "LC_CTYPE"_id, Token_Literal); 
    AddKeyword(lang, "LC_MONETARY"_id, Token_Literal);
    
    AddKeyword(lang, "LC_NUMERIC"_id, Token_Literal); 
    AddKeyword(lang, "LC_TIME"_id, Token_Literal); 
    AddKeyword(lang, "SIG_DFL"_id, Token_Literal); 
    AddKeyword(lang, "SIG_ERR"_id, Token_Literal); 
    AddKeyword(lang, "SIG_IGN"_id, Token_Literal); 
    AddKeyword(lang, "SIGABRT"_id, Token_Literal); 
    AddKeyword(lang, "SIGFPE"_id, Token_Literal); 
    AddKeyword(lang, "SIGILL"_id, Token_Literal); 
    AddKeyword(lang, "SIGHUP"_id, Token_Literal); 
    AddKeyword(lang, "SIGINT"_id, Token_Literal); 
    AddKeyword(lang, "SIGSEGV"_id, Token_Literal); 
    AddKeyword(lang, "SIGTERM"_id, Token_Literal);

    AddKeyword(lang, "M_E"_id, Token_Literal);
    AddKeyword(lang, "M_LOG2E"_id, Token_Literal);
    AddKeyword(lang, "M_LOG10E"_id, Token_Literal);
    AddKeyword(lang, "M_LN2"_id, Token_Literal);
    AddKeyword(lang, "M_LN10"_id, Token_Literal);
    AddKeyword(lang, "M_PI"_id, Token_Literal);
    AddKeyword(lang, "M_PI_2"_id, Token_Literal);
    AddKeyword(lang, "M_PI_4"_id, Token_Literal);

    AddKeyword(lang, "M_1_PI"_id, Token_Literal);
    AddKeyword(lang, "M_2_PI"_id, Token_Literal);
    AddKeyword(lang, "M_2_SQRTPI"_id, Token_Literal);
    AddKeyword(lang, "M_SQRT2"_id, Token_Literal);
    AddKeyword(lang, "M_SQRT1_2"_id, Token_Literal);
}
END_REGISTER_LANGUAGE
