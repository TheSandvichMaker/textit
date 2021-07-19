#ifndef TEXTIT_AUTO_INDENT_HPP
#define TEXTIT_AUTO_INDENT_HPP

enum_flags(uint8_t, IndentRule)
{
    IndentRule_PushIndent    = 0x1,
    IndentRule_PopIndent     = 0x2,
    IndentRule_Hanging       = 0x4,
    IndentRule_ForceLeft     = 0x8,
    IndentRule_Additive      = 0x10,

    IndentRule_AffectsIndent = 0xFF,
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
