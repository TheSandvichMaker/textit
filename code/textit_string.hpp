#ifndef TEXTIT_STRING_HPP
#define TEXTIT_STRING_HPP

typedef uint32_t StringSeparatorFlags;
enum
{
    StringSeparator_BeforeFirst = 0x1,
    StringSeparator_AfterLast = 0x2,
};

typedef uint32_t CharacterClassFlags;
enum CharacterClassFlags_ENUM : CharacterClassFlags
{
    Character_HorizontalWhitespace = 0x1,
    Character_VerticalWhitespace   = 0x2,
    Character_Whitespace           = Character_HorizontalWhitespace|Character_VerticalWhitespace,
    Character_Alphabetic           = 0x4,
    Character_Numeric              = 0x8,
    Character_Alphanumeric         = Character_Alphabetic|Character_Numeric,
    Character_Identifier           = 0x10|Character_Alphanumeric,
    Character_Utf8                 = 0x20,
};

typedef uint32_t StringMatchFlags;
enum StringMatchFlags_ENUM : StringMatchFlags
{
    StringMatch_CaseInsensitive = 0x1,
};

struct ParseUtf8Result
{
    uint32_t codepoint;
    uint32_t advance;
};

#endif /* TEXTIT_STRING_HPP */
