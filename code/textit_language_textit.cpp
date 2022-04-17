function void
TokenizeTextit(Tokenizer *tok, Token *t)
{
    ParseStandardToken(tok, t);
}

function void
ParseTagsTextit(Buffer *buffer)
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

BEGIN_REGISTER_LANGUAGE("textit", lang)
{
    AssociateExtension(lang, "textit"_str);

    lang->tokenize_userdata_size = sizeof(TokenizerCpp);
    lang->Tokenize  = TokenizeTextit;
    lang->ParseTags = ParseTagsTextit;

    AddOperator(lang, "#"_str, Token_LineComment);
    AddOperator(lang, "\""_str, Token_String);
}
END_REGISTER_LANGUAGE
