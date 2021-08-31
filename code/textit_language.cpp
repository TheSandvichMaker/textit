function LanguageSpec *
GetLanguageFromExtension(String ext)
{
    LanguageSpec *result = nullptr;
    for (LanguageSpec *lang = language_registry->first_language; lang; lang = lang->next)
    {
        for (int ext_index = 0; ext_index < lang->associated_extension_count; ext_index += 1)
        {
            if (AreEqual(lang->associated_extensions[ext_index], ext, StringMatch_CaseInsensitive))
            {
                result = lang;
                break;
            }
        }
    }
    return result;
}

function void
AssociateExtension(LanguageSpec *lang, String ext)
{
    Assert(lang->associated_extension_count < ArrayCount(lang->associated_extensions));
    lang->associated_extensions[lang->associated_extension_count++] = ext;
}

function void
AddKeyword(LanguageSpec *spec, StringID id, TokenKind kind)
{
    for (size_t i = 0; i < ArrayCount(spec->keyword_table); i += 1)
    {
        size_t slot_index = (id + i) % ArrayCount(spec->keyword_table);
        KeywordSlot slot = spec->keyword_table[slot_index]; 
        if (!slot.value)
        {
            spec->keyword_table[slot_index].id   = id;
            spec->keyword_table[slot_index].kind = kind;
            return;
        }
    }
    INVALID_CODE_PATH;
}

function void
AddOperator(LanguageSpec *spec, String pattern, TokenKind kind, TokenSubKind sub_kind = 0)
{
    Assert(pattern.size <= 4);
    Assert(spec->operator_count < ArrayCount(spec->operators));

    OperatorSlot *slot = &spec->operators[spec->operator_count++];
    slot->pattern = pattern.data[0];
    if (pattern.size >= 2) slot->pattern |= pattern.data[1] << 8;
    if (pattern.size >= 3) slot->pattern |= pattern.data[2] << 16;
    if (pattern.size == 4) slot->pattern |= pattern.data[3] << 24;
    slot->pattern_length = (uint8_t)pattern.size;
    slot->kind     = kind;
    slot->sub_kind = sub_kind;
}

function void
AddTokenSubKind(LanguageSpec *spec, TokenSubKind kind, StringID theme_id)
{
    Assert(kind >= 0 && kind < ArrayCount(spec->sub_token_kind_to_theme_id));
    spec->sub_token_kind_to_theme_id[kind] = theme_id;
}

function StringID
GetThemeIDForToken(LanguageSpec *spec, Token *t)
{
    StringID result     = GetBaseTokenThemeID(t->kind);
    StringID sub_result = spec->sub_token_kind_to_theme_id[t->sub_kind];
    if (sub_result)
    {
        result = sub_result;
    }
    return result;
}

function void
AddTagSubKind(LanguageSpec *spec, TagSubKind kind, String name, StringID theme_id)
{
    Assert(kind >= 0 && kind < ArrayCount(spec->sub_tag_kind_to_theme_id));
    spec->sub_tag_kind_name[kind]        = name;
    spec->sub_tag_kind_to_theme_id[kind] = theme_id;
}

function StringID
GetThemeIDForTag(LanguageSpec *spec, Tag *tag)
{
    StringID result = "text_foreground"_id;
    if (tag->kind == Tag_CommentAnnotation)
    {
        result = "text_comment_annotation"_id;
    }
    if (tag->sub_kind)
    {
        result = spec->sub_tag_kind_to_theme_id[tag->sub_kind];
    }
    return result;
}

function String
GetTagBaseKindName(Tag *tag)
{
    String result = "??"_str;
    switch (tag->kind)
    {
        case Tag_Declaration:       result = "decl"_str;  break;
        case Tag_Definition:        result = "defn"_str;  break;
        case Tag_CommentAnnotation: result = "annot"_str; break;
    }
    return result;
}

function String
GetTagSubKindName(LanguageSpec *spec, Tag *tag)
{
    String result = spec->sub_tag_kind_name[tag->sub_kind];
    return result;
}

function TokenKind
GetTokenKindFromStringID(LanguageSpec *spec, StringID id)
{
    TokenKind result = 0;
    uint64_t masked_id = (uint64_t)id & ((1ull << 56) - 1);
    for (size_t i = 0; i < ArrayCount(spec->keyword_table); i += 1)
    {
        size_t slot_index = (id + i) % ArrayCount(spec->keyword_table);
        KeywordSlot slot = spec->keyword_table[slot_index]; 
        if (!slot.value)
        {
            break;
        }
        else if (slot.id == masked_id)
        {
            result = slot.kind;
        }
    }
    return result;
}

function String
GetOperatorAsString(LanguageSpec *spec, TokenKind kind, TokenSubKind sub_kind = 0)
{
    String result = {};
    for (size_t i = 0; i < spec->operator_count; i += 1)
    {
        OperatorSlot *slot = &spec->operators[i];
        if (slot->kind == kind &&
            (!sub_kind || slot->sub_kind == sub_kind))
        {
            result = MakeString(slot->pattern_length, (uint8_t *)&slot->pattern);
            break;
        }
    }
    return result;
}
