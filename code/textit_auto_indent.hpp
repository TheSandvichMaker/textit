#ifndef TEXTIT_AUTO_INDENT_HPP
#define TEXTIT_AUTO_INDENT_HPP

enum_flags(uint8_t, IndentState)
{
    IndentState_BeginRegular = 0x1,
    IndentState_EndRegular   = 0x2,
    IndentState_BeginHanging = 0x4,
    IndentState_EndHanging   = 0x8,
    IndentState_ForceLeft    = 0x10,

    IndentState_BeginAny     = IndentState_BeginRegular|IndentState_BeginHanging,
    IndentState_EndAny       = IndentState_EndRegular|IndentState_EndHanging,
};

struct IndentRules
{
    IndentState unfinished_statement;
    IndentState states[Token_COUNT];
};

struct NestPair
{
    NestPair *next;
    TokenKind closing_token;
};

function TokenKind
GetOtherNestTokenKind(TokenKind kind)
{
    switch (kind)
    {
        case Token_LeftParen:  return Token_RightParen;
        case Token_RightParen: return Token_LeftParen;
        case Token_LeftScope:  return Token_RightScope;
        case Token_RightScope: return Token_LeftScope;
    }
    return Token_None;
}

#endif /* TEXTIT_AUTO_INDENT_HPP */
