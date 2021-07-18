#ifndef TEXTIT_AUTO_INDENT_HPP
#define TEXTIT_AUTO_INDENT_HPP

enum_flags(uint8_t, IndentRule)
{
    IndentRule_BeginRegular  = 0x1,
    IndentRule_EndRegular    = 0x2,
    IndentRule_BeginHanging  = 0x4,
    IndentRule_EndHanging    = 0x8,
    IndentRule_ForceLeft     = 0x10,
    IndentRule_Additive      = 0x20,

    IndentRule_BeginAny      = IndentRule_BeginRegular|IndentRule_BeginHanging,
    IndentRule_EndAny        = IndentRule_EndRegular|IndentRule_EndHanging,
    IndentRule_Regular       = IndentRule_BeginRegular|IndentRule_EndRegular,
    IndentRule_Hanging       = IndentRule_BeginHanging|IndentRule_EndHanging,
    IndentRule_AffectsIndent = IndentRule_BeginAny|IndentRule_EndAny,
};

struct IndentRules
{
    IndentRule unfinished_statement;
    IndentRule table[Token_COUNT];
};

function TokenKind
GetOtherNestTokenKind(TokenKind kind)
{
    switch (kind)
    {
        case Token_LeftParen:    return Token_RightParen;
        case Token_RightParen:   return Token_LeftParen;
        case Token_LeftScope:    return Token_RightScope;
        case Token_RightScope:   return Token_LeftScope;
        case Token_LineEnd:      return Token_StatementEnd;
        case Token_StatementEnd: return Token_LineEnd;
    }
    return Token_None;
}

#endif /* TEXTIT_AUTO_INDENT_HPP */
