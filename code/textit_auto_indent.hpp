#ifndef TEXTIT_AUTO_INDENT_HPP
#define TEXTIT_AUTO_INDENT_HPP

enum_flags(uint8_t, IndentRule)
{
    IndentRule_PushIndent    = 0x1,
    IndentRule_PopIndent     = 0x2,
    IndentRule_Hanging       = 0x4,
    IndentRule_ForceLeft     = 0x8,
    IndentRule_Additive      = 0x10,

    IndentRule_RequiresReindent = IndentRule_PopIndent|IndentRule_ForceLeft,
    IndentRule_AffectsIndent    = (IndentRule_Additive << 1) - 1,

    IndentRule_StatementEnd  = 0x20,
};

struct IndentRules
{
    IndentRule unfinished_statement;
    IndentRule table[Token_COUNT];
};

#endif /* TEXTIT_AUTO_INDENT_HPP */
