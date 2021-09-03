#ifndef TEXTIT_LANGUAGE_HPP_HPP
#define TEXTIT_LANGUAGE_HPP_HPP

enum TokenizeState_C
{
    TokenizeState_C_InPreprocessor = 0x1,
};

enum TokenKind_C : TokenSubKind
{
    Token_C_None,

    Token_C_Arrow,
    Token_C_PrintfFormatter,

    Token_Cpp_Namespace,
};

enum TagKind_C : TagSubKind
{
    Tag_C_None,

    Tag_C_Struct,
    Tag_C_Union,
    Tag_C_Enum,
    Tag_C_EnumValue,
    Tag_C_Typedef,
    Tag_C_Function,
    Tag_C_Macro,
    Tag_C_FunctionMacro,

    Tag_Cpp_Class,
};

struct TokenizerCpp
{
    uint8_t string_end_char;
};

#endif /* TEXTIT_LANGUAGE_HPP_HPP */
