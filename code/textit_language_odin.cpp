struct TokenizerOdin
{
	char string_end_char;
};

function void TokenizeOdin(Tokenizer *tok, Token *t)
{
    TokenizerOdin *tok_odin = (TokenizerOdin *)tok->userdata;

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
                if (c == 'L' && Match(tok, '"'))       { tok_odin->string_end_char = '"'; parse_string = true; } 
                if (c == 'u' && Match(tok, '"'))       { tok_odin->string_end_char = '"'; parse_string = true; }
                if (c == 'U' && Match(tok, '"'))       { tok_odin->string_end_char = '"'; parse_string = true; }
                if (c == 'u' && Match(tok, "8\""_str)) { tok_odin->string_end_char = '"'; parse_string = true; }
                if (HasFlag(tok->user_state, TokenizeState_C_InPreprocessor) && Peek(tok) == '<')
                {
                    ScopedMemory temp;
                    String prev_token_string = PushTokenString(temp, tok->buffer, prev_t);
                    if (AreEqual(prev_token_string, "include"_str))
                    {
                        parse_string = true;
                        tok_odin->string_end_char = '>';
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
            if (tok->in_string && tok_odin->string_end_char == '>')
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
                if (tok->in_string && tok_odin->string_end_char == '"')
                {
                    t->kind = Token_EndString;
                }
                else if (!tok->in_string)
                {
                    t->kind = Token_StartString;
                    tok_odin->string_end_char = '"';
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

		/*
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
		*/

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

function bool ConsumeOdinType(TagParser *parser)
{
    bool result = false;

    TokenLocator rewind_point = GetLocator(parser);

    ConsumeToken(parser, Token_Identifier);
	if (ConsumeToken(parser, '.'))
	{
		// That was a namespace, now consume the identifier.
		ConsumeToken(parser, Token_Identifier);
	}
	ConsumeBalancedPair(parser, Token_LeftParen);

    result = true;

// bail:
    if (!result)
    {
        Rewind(parser, rewind_point);
    }

    return result;
}

struct OdinPackage
{
	String name;
	String path;
	String full_path;
};

struct OdinLanguageState
{
	HashTable<OdinPackage> packages;
};

void ParseTagsOdin(Buffer *buffer)
{
    FreeAllTags(buffer);

	if (buffer->custom_language_state == nullptr)
	{
		buffer->custom_language_state = PushStruct(&buffer->arena, OdinLanguageState);
	}

    TagParser parser_;
    TagParser *parser = &parser_;
    InitializeTagParser(parser, buffer);

	OdinLanguageState *lang_state = (OdinLanguageState *)buffer->custom_language_state;
	
	// NestHelper scope_nest = {};

	String package_name = ""_str;

    while (TokensLeft(parser))
    {
        ScopedMemory temp;
		
		// Token peek_t = PeekToken(parser);
		// if (IsInNest(&scope_nest, peek_t.kind, Direction_Forward))
		// {
		// 	Advance(parser);
		// 	continue;
		// }

		String odin_path = "C:/Odin"_str; // TODO

        SetFlags(parser, 0, TokenFlag_IsComment|TokenFlag_IsPreprocessor);

		if (ConsumeToken(parser, Token_Keyword, "package"_str))
		{
			// TODO(daniel): This should only be accepted if it is on the same line
			Token tok_package_name = ConsumeToken(parser, Token_Identifier);
			package_name = PushTokenString(temp, buffer, &tok_package_name);
		}
		else if (ConsumeToken(parser, Token_Keyword, "import"_str))
		{
			if (Token tok_alias = ConsumeToken(parser, Token_Identifier))
			{
			}
			if (Token tok_path = ConsumeToken(parser, Token_String))
			{
				String package_path = PushTokenString(temp, buffer, &tok_path);

				String name;
				String collection = SplitAroundChar(package_path, ':', &name);

				bool is_new_package;
				OdinPackage *package = lang_state->packages.FindOrCreate(&buffer->arena, package_path, &is_new_package);

				if (is_new_package)
				{
					String full_package_path = PushStringF(&buffer->arena, "%.*s/%.*s/%.*s",
														   StringExpand(odin_path),
														   StringExpand(name),
														   StringExpand(collection));

					package->name = PushString(&buffer->arena, name);
					package->path = PushString(&buffer->arena, collection);
					package->full_path = full_package_path;

					OpenCodeFilesRecursively(full_package_path);
				}
			}
		}
        else if (Token ident = ConsumeToken(parser, Token_Identifier))
		{
			bool is_decl = false;
			bool is_const_decl = false;

			if (ConsumeToken(parser, Token_Operator, Token_Odin_ConstDeclaration))
			{
				is_decl = true;
				is_const_decl = true;
			}
			else if (ConsumeToken(parser, Token_Operator, Token_Odin_Declaration))
			{
				is_decl = true;
			}
			else if (ConsumeToken(parser, ':'))
			{
				ConsumeOdinType(parser);

				if (ConsumeToken(parser, ':'))
				{
					is_decl = true;
					is_const_decl = true;
				}
				else if (ConsumeToken(parser, '='))
				{
					is_decl = true;
				}
			}

			if (is_decl)
			{
		        TagSubKind match_kind = Tag_Odin_None;
		        if      (ConsumeToken(parser, Token_Keyword, "struct"_str)) match_kind = Tag_Odin_Struct;
		        else if (ConsumeToken(parser, Token_Keyword, "union"_str))  match_kind = Tag_Odin_Union;
		        else if (ConsumeToken(parser, Token_Keyword, "enum"_str))   match_kind = Tag_Odin_Enum;
		        else if (ConsumeToken(parser, Token_Keyword, "proc"_str))   match_kind = Tag_Odin_Proc;

				if (match_kind != Tag_Odin_None)
				{
					String token_name = PushTokenString(temp, buffer, &ident);

					Tag *tag = AddTag(buffer, &ident);
					tag->kind     = Tag_Declaration;
					tag->sub_kind = match_kind;

					if (match_kind == Tag_Odin_Proc)
					{
						ConsumeBalancedPair(parser, Token_LeftParen);
						if (ConsumeToken(parser, Token_Operator, Token_Odin_Uninitialized))
						{
						}
						else
						{
							ConsumeBalancedPair(parser, Token_LeftScope);
						}
					}
					else
					{
						ConsumeBalancedPair(parser, Token_LeftScope);
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

BEGIN_REGISTER_LANGUAGE("odin", lang)
{
    AssociateExtension(lang, "odin"_str);
	
    lang->tokenize_userdata_size = sizeof(TokenizerOdin);
    lang->Tokenize           = TokenizeOdin;
    lang->ParseTags          = ParseTagsOdin;
    lang->CustomAutocomplete = nullptr;

    // AddTokenSubKind(lang, Token_C_StringSpecial, "text_string_special"_id);

    AddTagSubKind(lang, Tag_Odin_Struct,        "struct"_str,         "text_type"_id);
    AddTagSubKind(lang, Tag_Odin_Union,         "union"_str,          "text_type"_id);
    AddTagSubKind(lang, Tag_Odin_Enum,          "enum"_str,           "text_type"_id);
    AddTagSubKind(lang, Tag_Odin_EnumValue,     "enum_value"_str,     "text_literal"_id);
    AddTagSubKind(lang, Tag_Odin_Proc,          "proc"_str,           "text_function"_id);
    AddTagSubKind(lang, Tag_Odin_Constant,      "constant"_str,       "text_macro"_id);

    AddOperator(lang, "{"_str, Token_LeftScope);
    AddOperator(lang, "}"_str, Token_RightScope);
    AddOperator(lang, "("_str, Token_LeftParen);
    AddOperator(lang, ")"_str, Token_RightParen);
    AddOperator(lang, "\\"_str, Token_LineContinue);

    AddOperator(lang, "/*"_str, Token_OpenBlockComment);
    AddOperator(lang, "*/"_str, Token_CloseBlockComment);
    AddOperator(lang, "//"_str, Token_LineComment);

    AddOperator(lang, "\""_str, Token_String);

    AddOperator(lang, "::"_str, Token_Operator, Token_Odin_ConstDeclaration);
    AddOperator(lang, ":="_str, Token_Operator, Token_Odin_Declaration);
    AddOperator(lang, "->"_str, Token_Operator, Token_Odin_Arrow);
    AddOperator(lang, "---"_str, Token_Operator, Token_Odin_Uninitialized);

    AddKeyword(lang, "package"_id, Token_Keyword);
    AddKeyword(lang, "import"_id, Token_Keyword);
    AddKeyword(lang, "struct"_id, Token_Keyword);
    AddKeyword(lang, "union"_id, Token_Keyword);
    AddKeyword(lang, "enum"_id, Token_Keyword);
    AddKeyword(lang, "using"_id, Token_Keyword);
    AddKeyword(lang, "proc"_id, Token_Keyword);

    AddKeyword(lang, "case"_id, Token_FlowControl);
    AddKeyword(lang, "default"_id, Token_FlowControl);
    AddKeyword(lang, "continue"_id, Token_FlowControl);
    AddKeyword(lang, "break"_id, Token_FlowControl);
    AddKeyword(lang, "if"_id, Token_FlowControl);
    AddKeyword(lang, "else"_id, Token_FlowControl);
    AddKeyword(lang, "for"_id, Token_FlowControl);
    AddKeyword(lang, "switch"_id, Token_FlowControl);
    AddKeyword(lang, "return"_id, Token_FlowControl);
    AddKeyword(lang, "defer"_id, Token_FlowControl);

    AddKeyword(lang, "bool"_id, Token_Type); 
    AddKeyword(lang, "rune"_id, Token_Type); 
    AddKeyword(lang, "f32"_id, Token_Type); 
    AddKeyword(lang, "f64"_id, Token_Type); 
    AddKeyword(lang, "i8"_id, Token_Type); 
    AddKeyword(lang, "i16"_id, Token_Type); 
    AddKeyword(lang, "i32"_id, Token_Type); 
    AddKeyword(lang, "i64"_id, Token_Type);
    AddKeyword(lang, "u8"_id, Token_Type); 
    AddKeyword(lang, "u16"_id, Token_Type); 
    AddKeyword(lang, "u32"_id, Token_Type); 
    AddKeyword(lang, "u64"_id, Token_Type);
    AddKeyword(lang, "uintptr"_id, Token_Type);

    AddKeyword(lang, "true"_id, Token_Literal);
    AddKeyword(lang, "false"_id, Token_Literal);

    AddKeyword(lang, "nil"_id, Token_Literal);
}
END_REGISTER_LANGUAGE
