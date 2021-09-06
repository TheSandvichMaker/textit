function void
TokenizeLua(Tokenizer *tok, Token *t)
{
    Token *prev_t = tok->prev_token;

    uint8_t c = Advance(tok);
    switch (c)
    {
        default:
        {
parse_default:
            Revert(tok, t);
            ParseStandardToken(tok, t);
        } break;

        case '{':
        case '(':
        {
            if (prev_t->kind == Token_Identifier)
            {
                prev_t->kind = Token_Function;
            }
            goto parse_default;
        } break;
    }
}

function void
ParseTagsLua(Buffer *buffer)
{
    FreeAllTags(buffer);

    TagParser parser_;
    TagParser *parser = &parser_;
    InitializeTagParser(parser, buffer);

    while (TokensLeft(parser))
    {
        ParseBuiltinTag(parser);
    }
}

BEGIN_REGISTER_LANGUAGE("lua", lang)
{
    AssociateExtension(lang, "lua"_str);

    lang->tokenize_userdata_size = 1;
    lang->Tokenize  = TokenizeLua;
    lang->ParseTags = ParseTagsLua;

    AddOperator(lang, "{"_str, Token_LeftScope);
    AddOperator(lang, "}"_str, Token_RightScope);
    AddOperator(lang, "("_str, Token_LeftParen);
    AddOperator(lang, ")"_str, Token_RightParen);
    AddOperator(lang, "\\"_str, Token_LineContinue);

    // AddOperator(lang, "/*"_str, Token_OpenBlockComment);
    // AddOperator(lang, "*/"_str, Token_CloseBlockComment);
    AddOperator(lang, "--"_str, Token_LineComment);

    AddOperator(lang, "\""_str, Token_String);

    AddKeyword(lang, "then"_id, Token_LeftScope);
    AddKeyword(lang, "do"_id, Token_LeftScope);
    AddKeyword(lang, "function"_id, Token_LeftScope);

    AddKeyword(lang, "end"_id, Token_RightScope);

    AddKeyword(lang, "local"_id, Token_Keyword);

    AddKeyword(lang, "continue"_id, Token_FlowControl);
    AddKeyword(lang, "break"_id, Token_FlowControl);
    AddKeyword(lang, "if"_id, Token_FlowControl);
    AddKeyword(lang, "else"_id, Token_FlowControl);
    AddKeyword(lang, "for"_id, Token_FlowControl);
    AddKeyword(lang, "while"_id, Token_FlowControl);
    AddKeyword(lang, "do"_id, Token_FlowControl);
    AddKeyword(lang, "goto"_id, Token_FlowControl);
    AddKeyword(lang, "return"_id, Token_FlowControl);
}
END_REGISTER_LANGUAGE
