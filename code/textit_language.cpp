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

function LanguageSpec *
PushCppLanguageSpec(void)
{
    LanguageSpec *result = PushStruct(&editor->transient_arena, LanguageSpec);

    result->name = "cpp"_str;
    result->associated_extensions[result->associated_extension_count++] = "cpp"_str;
    result->associated_extensions[result->associated_extension_count++] = "hpp"_str;
    result->associated_extensions[result->associated_extension_count++] = "C"_str;
    result->associated_extensions[result->associated_extension_count++] = "cc"_str;

    AddKeyword(result, "static"_id, Token_Keyword);
    AddKeyword(result, "inline"_id, Token_Keyword);
    AddKeyword(result, "operator"_id, Token_Keyword);
    AddKeyword(result, "volatile"_id, Token_Keyword);
    AddKeyword(result, "extern"_id, Token_Keyword);
    AddKeyword(result, "const"_id, Token_Keyword);
    AddKeyword(result, "struct"_id, Token_Keyword);
    AddKeyword(result, "enum"_id, Token_Keyword);
    AddKeyword(result, "class"_id, Token_Keyword);
    AddKeyword(result, "public"_id, Token_Keyword);
    AddKeyword(result, "private"_id, Token_Keyword);
    AddKeyword(result, "thread_local"_id, Token_Keyword);
    AddKeyword(result, "final"_id, Token_Keyword);
    AddKeyword(result, "constexpr"_id, Token_Keyword);
    AddKeyword(result, "consteval"_id, Token_Keyword);

    AddKeyword(result, "case"_id, Token_FlowControl);
    AddKeyword(result, "continue"_id, Token_FlowControl);
    AddKeyword(result, "break"_id, Token_FlowControl);
    AddKeyword(result, "if"_id, Token_FlowControl);
    AddKeyword(result, "else"_id, Token_FlowControl);
    AddKeyword(result, "for"_id, Token_FlowControl);
    AddKeyword(result, "switch"_id, Token_FlowControl);
    AddKeyword(result, "while"_id, Token_FlowControl);
    AddKeyword(result, "do"_id, Token_FlowControl);
    AddKeyword(result, "goto"_id, Token_FlowControl);
    AddKeyword(result, "return"_id, Token_FlowControl);

    AddKeyword(result, "true"_id, Token_Literal);
    AddKeyword(result, "false"_id, Token_Literal);
    AddKeyword(result, "nullptr"_id, Token_Literal);
    AddKeyword(result, "NULL"_id, Token_Literal);

    AddKeyword(result, "unsigned"_id, Token_Type);
    AddKeyword(result, "signed"_id, Token_Type);
    AddKeyword(result, "register"_id, Token_Type);
    AddKeyword(result, "bool"_id, Token_Type); 
    AddKeyword(result, "void"_id, Token_Type); 
    AddKeyword(result, "char"_id, Token_Type); 
    AddKeyword(result, "char_t"_id, Token_Type); 
    AddKeyword(result, "char16_t"_id, Token_Type); 
    AddKeyword(result, "char32_t"_id, Token_Type); 
    AddKeyword(result, "wchar_t"_id, Token_Type); 
    AddKeyword(result, "short"_id, Token_Type); 
    AddKeyword(result, "int"_id, Token_Type); 
    AddKeyword(result, "long"_id, Token_Type); 
    AddKeyword(result, "float"_id, Token_Type); 
    AddKeyword(result, "double"_id, Token_Type);
    AddKeyword(result, "int8_t"_id, Token_Type); 
    AddKeyword(result, "int16_t"_id, Token_Type); 
    AddKeyword(result, "int32_t"_id, Token_Type); 
    AddKeyword(result, "int64_t"_id, Token_Type);
    AddKeyword(result, "uint8_t"_id, Token_Type); 
    AddKeyword(result, "uint16_t"_id, Token_Type); 
    AddKeyword(result, "uint32_t"_id, Token_Type); 
    AddKeyword(result, "uint64_t"_id, Token_Type);
    AddKeyword(result, "intptr_t"_id, Token_Type);
    AddKeyword(result, "uintptr_t"_id, Token_Type);
    AddKeyword(result, "size_t"_id, Token_Type);
    AddKeyword(result, "ssize_t"_id, Token_Type);
    AddKeyword(result, "ptrdiff_t"_id, Token_Type);

    SllStackPush(editor->first_language, result);

    return result;
}
