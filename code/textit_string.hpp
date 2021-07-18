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

template <size_t Capacity>
struct FixedStringContainer
{
    static constexpr size_t capacity = Capacity;
    size_t size = 0;
    uint8_t data[Capacity];

    String
    AsString()
    {
        return MakeString(size, data);
    }

    bool
    CanFitAppend(String string)
    {
        size_t left = capacity - size;
        return left >= string.size;
    }

    void
    Append(String string)
    {
        size_t left = capacity - size;
        size_t to_copy = Min(left, string.size);

        CopyArray(to_copy, string.data, data + size);
        size += to_copy;
    }

    void
    Append(uint8_t character)
    {
        if (size < capacity)
        {
            data[size++] = character;
        }
    }

    void
    AppendFill(size_t count, uint8_t character)
    {
        size_t left = capacity - size;
        if (count > left) count = left;

        uint8_t *at = data + size;
        for (size_t i = 0; i < count; i += 1)
        {
            *at++ = character;
        }
        size += count;
    }

    void
    Clear()
    {
        size = 0;
    }
};

#endif /* TEXTIT_STRING_HPP */
