//
// Ascii
//

static inline bool
IsPrintableAscii(uint8_t c)
{
    bool result = (c >= ' ' && c <= '~');
    return result;
}

static inline bool
IsAlphabeticAscii(uint8_t c)
{
    bool result = ((c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z'));
    return result;
}

static inline bool
IsNumericAscii(uint8_t c)
{
    bool result = ((c >= '0') && (c <= '9'));
    return result;
}

static inline bool
IsAlphanumericAscii(uint8_t c)
{
    bool result = (IsAlphabeticAscii(c) ||
                   IsNumericAscii(c));
    return result;
}

static inline bool
IsWhitespaceAscii(uint8_t c)
{
    bool result = ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
    return result;
}

//
// Unicode party
//

/*
static const uint8_t utf8_mask[]           = { 0x80, 0xE0, 0xF0, 0xF8, };
static const uint8_t utf8_matching_value[] = { 0x00, 0xC0, 0xE0, 0xF0, };
*/

static inline bool
IsAsciiByte(uint8_t b)
{
    if ((b & 0x80) == 0x00) return true;
    return false;
}

static inline int
IsHeadUnicodeByte(uint8_t b)
{
    if ((b & 0xE0) == 0xC0) return true;
    if ((b & 0xF0) == 0xE0) return true;
    if ((b & 0xF8) == 0xF0) return true;
    return false;
}

static inline bool
IsTrailingUnicodeByte(uint8_t b)
{
    if ((b & 0xC0) == 0x80)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static inline bool
IsUnicodeByte(uint8_t b)
{
    return (IsHeadUnicodeByte(b) ||
            IsTrailingUnicodeByte(b));
}

static inline ParseUtf8Result
ParseUtf8Codepoint(uint8_t *text)
{
    // TODO: This is adapted old code, I can't guarantee its quality, but I think it is at least correct

    ParseUtf8Result result = {};

    uint32_t num_bytes = 0;

    uint8_t *at = text;
    uint32_t codepoint = 0;

    /*
    uint8_t utf8_mask[]           = { 0x80, 0xE0, 0xF0, 0xF8, };
    uint8_t utf8_matching_value[] = { 0x00, 0xC0, 0xE0, 0xF0, };
    */

    if (!at[0])
    {
        // @Note: We've been null terminated.
    }
    else
    {
        uint8_t utf8_mask[] =
        {
            (1 << 7) - 1,
            (1 << 5) - 1,
            (1 << 4) - 1,
            (1 << 3) - 1,
        };
        uint8_t utf8_matching_value[] = { 0, 0xC0, 0xE0, 0xF0 };
        if ((*at & ~utf8_mask[0]) == utf8_matching_value[0])
        {
            num_bytes = 1;
        }
        else if ((uint8_t)(*at & ~utf8_mask[1]) == utf8_matching_value[1])
        {
            num_bytes = 2;
        }
        else if ((uint8_t)(*at & ~utf8_mask[2]) == utf8_matching_value[2])
        {
            num_bytes = 3;
        }
        else if ((uint8_t)(*at & ~utf8_mask[3]) == utf8_matching_value[3])
        {
            num_bytes = 4;
        }
        else
        {
            /* This is some nonsense input */
            num_bytes = 0;
        }

        if (num_bytes)
        {
            uint32_t offset = 6*(num_bytes - 1);
            for (size_t byte_index = 0; byte_index < num_bytes; byte_index++)
            {
                if (byte_index == 0)
                {
                    codepoint = (*at & utf8_mask[num_bytes-1]) << offset;
                }
                else
                {
                    if (*at != 0)
                    {
                        codepoint |= (*at & ((1 << 6) - 1)) << offset;
                    }
                    else
                    {
                        /* This is some nonsense input */
                        codepoint = 0;
                        break;
                    }
                }
                offset -= 6;
                at++;
            }
        }
    }

    result.codepoint = codepoint;
    result.advance = num_bytes;

    return result;
}

static bool
AreEqual(const String &a, const String &b)
{
    bool result = false;

    if (a.size == b.size)
    {
        result = true;
        for (size_t i = 0; i < a.size; ++i)
        {
            char c1 = a.data[i];
            char c2 = b.data[i];

            if (c1 != c2)
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

static inline String
FormatStringV(Arena *arena, char *fmt, va_list args_init)
{
    va_list args_size;
    va_copy(args_size, args_init);

    va_list args_fmt;
    va_copy(args_fmt, args_init);

    int chars_required = vsnprintf(nullptr, 0, fmt, args_size) + 1;
    va_end(args_size);

    String result = {};
    result.size = chars_required - 1;
    result.data = PushArrayNoClear(arena, chars_required, uint8_t);
    vsnprintf((char *)result.data, chars_required, fmt, args_fmt);
    va_end(args_fmt);

    return result;
}

static inline String
FormatString(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = FormatStringV(arena, fmt, args);
    va_end(args);
    return result;
}

static inline String
FormatTempString(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = FormatStringV(GetTempArena(), fmt, args);
    va_end(args);
    return result;
}

static inline String
PushString(Arena *arena, String string)
{
    String result = {};
    result.size = string.size;
    result.data = PushArrayNoClear(arena, result.size, uint8_t);
    CopyArray(string.size, string.data, result.data);
    return result;
}
