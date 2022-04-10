//
// Ascii
//

function bool
IsPrintableAscii(uint8_t c)
{
    bool result = (c >= ' ' && c <= '~');
    return result;
}

function bool
IsAlphabeticAscii(uint8_t c)
{
    bool result = ((c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z'));
    return result;
}

function uint8_t
ToUpperAscii(uint8_t c)
{
    if ((c >= 'a') && (c <= 'z'))
    {
        c = 'A' + (c - 'a');
    }
    return c;
}

function uint8_t
ToLowerAscii(uint8_t c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        c = 'a' + (c - 'A');
    }
    return c;
}

function bool
IsNumericAscii(uint8_t c)
{
    bool result = ((c >= '0') && (c <= '9'));
    return result;
}

function bool
IsAlphanumericAscii(uint8_t c)
{
    bool result = (IsAlphabeticAscii(c) ||
                   IsNumericAscii(c));
    return result;
}

function bool
IsValidIdentifierAscii(uint8_t c)
{
    bool result = (IsAlphanumericAscii(c) ||
                   c == '_');
    return result;
}

function bool
IsWhitespaceAscii(uint8_t c)
{
    bool result = ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
    return result;
}

function bool
IsHorizontalWhitespaceAscii(uint8_t c)
{
    bool result = ((c == ' ') || (c == '\t'));
    return result;
}

function bool
IsVerticalWhitespaceAscii(uint8_t c)
{
    bool result ((c == '\r') || (c == '\n'));
    return result;
}

//
// Unicode party
//

/*
static const uint8_t utf8_mask[]           = { 0x80, 0xE0, 0xF0, 0xF8, };
static const uint8_t utf8_matching_value[] = { 0x00, 0xC0, 0xE0, 0xF0, };
*/

function bool
IsAsciiByte(uint8_t b)
{
    if ((b & 0x80) == 0x00) return true;
    return false;
}

function int
IsHeadUtf8Byte(uint8_t b)
{
    if ((b & 0xE0) == 0xC0) return true;
    if ((b & 0xF0) == 0xE0) return true;
    if ((b & 0xF8) == 0xF0) return true;
    return false;
}

function bool
IsTrailingUtf8Byte(uint8_t b)
{
    return !!((b & 0xC0) == 0x80);
}

function bool
IsUtf8Byte(uint8_t b)
{
    return b >= 128;
}

function CharacterClassFlags
CharacterizeByte(uint8_t c)
{
    CharacterClassFlags result = 0;
    if (IsHorizontalWhitespaceAscii(c)) result |= Character_HorizontalWhitespace;
    if (IsVerticalWhitespaceAscii(c)) result |= Character_VerticalWhitespace;
    if (IsAlphabeticAscii(c)) result |= Character_Alphabetic;
    if (IsNumericAscii(c)) result |= Character_Numeric;
    if (c == '_') result |= 0x10; // todo: waddup
    if (IsUtf8Byte(c)) result |= Character_Utf8;
    return result;
}

function CharacterClassFlags
CharacterizeByteLoosely(uint8_t c)
{
    CharacterClassFlags result = 0;
    if (IsHorizontalWhitespaceAscii(c)) result |= Character_HorizontalWhitespace;
    if (IsVerticalWhitespaceAscii(c)) result |= Character_VerticalWhitespace;
    if (IsAlphabeticAscii(c)) result |= Character_Identifier;
    if (IsNumericAscii(c)) result |= Character_Identifier;
    if (c == '_') result |= Character_Identifier; // todo: waddup
    if (IsUtf8Byte(c)) result |= Character_Utf8;
    return result;
}

function ParseUtf8Result
ParseUtf8Codepoint(uint8_t *text)
{
    // TODO: This is adapted old code, I can't guarantee its quality, but I think it is at least correct

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

    return { codepoint, num_bytes };
}

function bool
AreEqual(const String &a, const String &b, StringMatchFlags flags = 0)
{
    bool result = false;

    if (a.size == b.size)
    {
        result = true;
        for (size_t i = 0; i < a.size; ++i)
        {
            char c1 = a.data[i];
            char c2 = b.data[i];

            if (flags & StringMatch_CaseInsensitive)
            {
                c1 = ToLowerAscii(c1);
                c2 = ToLowerAscii(c2);
            }

            if (c1 != c2)
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

function String
SplitExtension(String string, String *right)
{
    String result = string;
    if (right)
    {
        right->data = string.data;
        right->size = 0;
    }
    if (string.size > 0)
    {
        for (size_t i = 0; i < string.size ; i += 1)
        {
            if (string.data[i] == '.')
            {
                result.data = string.data;
                result.size = i;
                if (right && i + 1 < string.size)
                {
                    right->data = string.data + i + 1;
                    right->size = string.size - i - 1;
                }
            }
        }
    }
    return result;
}

function String
SplitPath(String string, String *leaf = nullptr)
{
    String result = string;
    result.size   = 0;
    for (size_t i = 0; i < string.size; i += 1)
    {
        if (string.data[i] == '/' ||
            string.data[i] == '\\')
        {
            result.size = i + 1;
        }
    }
    if (leaf)
    {
        leaf->data = string.data + result.size;
        leaf->size = string.size - result.size;
    }
    return result;
}

function uint8_t
Peek(String string, size_t index)
{
    if (index < string.size)
    {
        return string.data[index];
    }
    return 0;
}

function int
PeekNewline(String string, size_t index)
{
    int result = 0;
    if (Peek(string, index) == '\n')
    {
        result = 1;
    }
    else if (Peek(string, index)     == '\r' &&
             Peek(string, index + 1) == '\n')
    {
        result = 2;
    }
    return result;
}

function uint8_t
PeekEnd(String string)
{
    uint8_t result = 0;
    if (string.size > 0)
    {
        result = string.data[string.size - 1];
    }
    return result;
}

function String
Advance(String string, size_t by = 1)
{
    if (by <= string.size)
    {
        string.size -= by;
        string.data += by;
    }
    else
    {
        string.data += string.size;
        string.size = 0;
    }
    return string;
}

function String
TrimSpaces(String string)
{
    String result = string;
    while (result.size > 0 && IsWhitespaceAscii(result.data[0]))
    {
        result.data += 1;
        result.size -= 1;
    }
    while (result.size > 0 && IsWhitespaceAscii(result.data[result.size - 1]))
    {
        result.size -= 1;
    }
    return result;
}

static const struct CharToDigitValues
{
    uint8_t values[256] = {};
    constexpr CharToDigitValues()
    {
        values['0'] = 0;
        values['1'] = 1;
        values['2'] = 2;
        values['3'] = 3;
        values['4'] = 4;
        values['5'] = 5;
        values['6'] = 6;
        values['7'] = 7;
        values['8'] = 8;
        values['9'] = 9;
        values['a'] = 10; values['A'] = 10;
        values['b'] = 11; values['B'] = 11;
        values['c'] = 12; values['C'] = 12;
        values['d'] = 13; values['D'] = 13;
        values['e'] = 14; values['E'] = 14;
        values['f'] = 15; values['F'] = 15;
    }
} char_to_digit;

function bool
ParseInt(String in_string, String *out_string, int64_t *out_value)
{
    bool result = false;

    size_t i = 0;
    String string = in_string;

    int64_t sign = 1;
    for (; i < string.size; i += 1)
    {
        if      (string.data[i] == '-') { sign = -1; }
        else if (string.data[i] == '+') { sign =  1; }
        else break;
    }

    int64_t base = 10;
    if (string.data[i] == '0')
    { 
        base = 8; i += 1; 
        if (string.data[i] == 'x' || string.data[i] == 'X') 
        {
            base = 16; i += 1; 
        }
        else if (string.data[i] == 'b')
        {
            base = 2; i += 1;
        }
    }

    string.data += i;
    string.size -= i;
    i = 0;

    int64_t value = 0;
    for (; i < string.size; i += 1)
    {
        uint8_t c = string.data[i];
        int64_t digit = char_to_digit.values[c];
        if (digit == 0 && c != '0')
        {
            // non-numeric character
            break;
        }
        if (digit >= base)
        {
            // digit out of range
            break;
        }
        if (value > (INT64_MAX - digit) / base)
        {
            // overflow
            i = 0;
            break;
        }
        value *= base;
        value += digit;
    }

    value *= sign;

    if (i != 0)
    {
        result = true;
        *out_value = value;
        if (out_string)
        {
            out_string->data = string.data + i;
            out_string->size = string.size - i;
        }
    }

    return result;
}

function String
SplitLine(String string, String *rhs = nullptr)
{
    String result = string;
    if (rhs)
    {
        rhs->data = result.data;
        rhs->size = 0;
    }
    for (size_t i = 0; i < string.size; i += 1)
    {
        if (int nl = PeekNewline(string, i))
        {
            result.size = i;
            if (rhs)
            {
                rhs->data = result.data + result.size + nl;
                rhs->size = string.size - result.size - nl;
            }
            break;
        }
    }
    return result;
}

function String
SplitWord(String string, String *rhs = nullptr)
{
    size_t i = 0;
    while (IsWhitespaceAscii(Peek(string, i))) i += 1;

    String result;
    result.data = string.data + i;

    size_t j = i;

    CharacterClassFlags skip_class = CharacterizeByteLoosely(Peek(string, j));
    while (j < string.size && CharacterizeByteLoosely(Peek(string, j)) == skip_class) j += 1;

    result.size = j - i;

    if (rhs)
    {
        rhs->data = result.data + result.size;
        rhs->size = string.size - result.size;
    }

    return result;
}

function String
SplitAroundChar(String string, uint8_t c, String *rhs = nullptr)
{
    size_t i;
    for (i = 0; i < string.size; i += 1)
    {
        if (string.data[i] == c)
        {
            break;
        }
    }

    String result;
    result.data = string.data;
    result.size = i;

    if (rhs)
    {
        rhs->data = string.data + result.size + 1;
        rhs->size = string.size - result.size - 1;
    }

    return result;
}

function bool
MatchPrefix(String string, String prefix, StringMatchFlags flags = 0)
{
    if (prefix.size > string.size) return false;
    if (string.size > prefix.size) string.size = prefix.size;
    return AreEqual(string, prefix, flags);
}

function size_t
Find(String str, uint8_t c)
{
    size_t result = str.size;

    for (size_t i = 0; i < str.size; i++)
    {
        if (str.data[i] == c)
        {
            result = i;
            break;
        }
    }

    return result;
}

function size_t
FindSubstring(String text, String pattern, StringMatchFlags flags = 0)
{
    size_t m = pattern.size;
    size_t R = ~1ull;
    size_t pattern_mask[256];

    if (m == 0) return 0;
    if (m >= 8*sizeof(size_t)) return text.size; // too long

    for (size_t i = 0; i < 256; i += 1) pattern_mask[i] = ~0ull;
    for (size_t i = 0; i < m;   i += 1)
    {
        uint8_t c = pattern.data[i];
        if (flags & StringMatch_CaseInsensitive) c = ToLowerAscii(c);
        pattern_mask[c] &= ~(1ull << i);
    }

    for (size_t i = 0; i < text.size; i += 1)
    {
        uint8_t c = text.data[i];
        if (flags & StringMatch_CaseInsensitive) c = ToLowerAscii(c);
        R |= pattern_mask[c];
        R <<= 1;

        if ((R & (1ull << m)) == 0)
        {
            return i - m + 1;
        }
    }

    return text.size;
}

function size_t
FindSubstringBackward(String text, String pattern, StringMatchFlags flags = 0)
{
    size_t m = pattern.size;
    size_t R = ~1ull;
    size_t pattern_mask[256];

    if (m == 0) return 0;
    if (m >= 8*sizeof(size_t)) return text.size; // too long

    for (size_t i = 0; i < 256; i += 1) pattern_mask[i] = ~0ull;
    for (size_t i = 0; i < m;   i += 1)
    {
        uint8_t c = pattern.data[m - i - 1];
        if (flags & StringMatch_CaseInsensitive) c = ToLowerAscii(c);
        pattern_mask[c] &= ~(1ull << i);
    }

    for (size_t i = 0; i < text.size; i += 1)
    {
        uint8_t c = text.data[text.size - i - 1];
        if (flags & StringMatch_CaseInsensitive) c = ToLowerAscii(c);
        R |= pattern_mask[c];
        R <<= 1;

        if ((R & (1ull << m)) == 0)
        {
            return text.size - i - 1;
        }
    }

    return text.size;
}

function int
CalculateEditDistance(String s, String t)
{
    // Levenshtein distance
    int n = (int)s.size;
    int m = (int)t.size;
    int stride = n + 1;

    ScopedMemory temp;
    int *matrix = PushArray(temp, (n + 1)*(m + 1), int);

    auto SetCell = [matrix, stride](int x, int y, int value)
    {
        matrix[y*stride + x] = value;
    };

    auto GetCell = [matrix, stride](int x, int y)
    {
        return matrix[y*stride + x];
    };

    auto Min3 = [](int a, int b, int c)
    {
        int result = a;
        if (b < result) result = b;
        if (c < result) result = c;
        return result;
    };

    for (int i = 1; i <= n; i += 1) SetCell(i, 0, i);
    for (int i = 1; i <= m; i += 1) SetCell(0, i, i);

    for (int i = 1; i <= n; i += 1)
    {
        uint8_t s_i = s.data[i - 1];
        for (int j = 1; j <= m; j += 1)
        {
            uint8_t t_j = t.data[j - 1];

            int cost = (s_i == t_j ? 0 : 1);
            int value = Min3(GetCell(i - 1, j) + 1,
                             GetCell(i, j - 1) + 1,
                             GetCell(i - 1, j - 1) + cost);
            SetCell(i, j, value);
        }
    }

    return GetCell(n, m);
}

function int
CountNewlines(String string)
{
    int result = 0;
    for (size_t i = 0; i < string.size; i += 1)
    {
        if (string[i] == '\r' || string[i] == '\n')
        {
            i += (string[i] + string[i + 1] == '\r' + '\n' ? 1 : 0);
            result += 1;
        }
    }
    return result;
}

function size_t
CountNullTerminatedString(char *string, size_t max = 0)
{
    size_t result = 0;
    while (*string++ && (max == 0 || result < max))
    {
        result += 1;
    }
    return result;
}

function String
PushString(Arena *arena, String string)
{
    String result = {};
    result.size = string.size;
    result.data = PushArray(arena, result.size + 1, uint8_t);
    CopySize(result.size, string.data, result.data);
    result.data[result.size] = 0;
    return result;
}

function String
PushStringSpace(Arena *arena, int64_t size)
{
    String result = {};
    result.size = size;
    result.data = PushArray(arena, result.size + 1, uint8_t);
    result.data[result.size] = 0;
    return result;
}

function size_t
FormatStringBuffer(size_t size, uint8_t *buffer, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t result = vsnprintf((char *const)buffer, size, fmt, args);
    va_end(args);
    return result;
}

function String
PushStringFV(Arena *arena, const char *fmt, va_list args_init)
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

function String
PushStringF(Arena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = PushStringFV(arena, fmt, args);
    va_end(args);
    return result;
}

function String
PushTempStringF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = PushStringFV(platform->GetTempArena(), fmt, args);
    va_end(args);
    return result;
}

function String
FormatHumanReadableBytes(size_t bytes)
{
    size_t log = 0;
    for (size_t x = bytes / 1024; x > 0; x /= 1024) log += 1;

    String string = {};
    if (log == 0)
    {
        string = PushTempStringF("%zuB", bytes);
    }
    else if (log == 1)
    {
        string = PushTempStringF("%zuKiB", bytes / 1024);
    }
    else if (log == 2)
    {
        string = PushTempStringF("%zuMiB", bytes / 1024 / 1024);
    }
    else if (log == 3)
    {
        string = PushTempStringF("%zuGiB", bytes / 1024 / 1024 / 1024);
    }
    else if (log >= 4)
    {
        string = PushTempStringF("%zuTiB", bytes / 1024 / 1024 / 1024 / 1024);
    }
    return string;
}

function String
LineEndString(LineEndKind kind)
{
    switch (kind)
    {
        case LineEnd_LF:   return StringLiteral("\n");
        case LineEnd_CRLF: return StringLiteral("\r\n");
    }
    return StringLiteral("");
}

function LineEndKind
GuessLineEndKind(String string)
{
    int64_t lf   = 0;
    int64_t crlf = 0;
    for (size_t i = 0; i < string.size; i += 1)
    {
        if (string.data[i + 0] == '\r' &&
            string.data[i + 1] == '\n')
        {
            crlf += 1;
            i += 1;
        }
        else if (string.data[i] == '\n')
        {
            lf += 1;
        }
    }

    LineEndKind result = LineEnd_LF;
    if (crlf > lf)
    {
        result = LineEnd_CRLF;
    }
    return result;
}

//
// StringContainer
//

function size_t
GetSizeLeft(StringContainer *container)
{
    size_t left = container->capacity - container->size;
    return left;
}

function bool
IsEmpty(StringContainer *container)
{
    return container->size == 0;
}

function bool
CanFitAppend(StringContainer *container, String string)
{
    size_t left = container->capacity - container->size;
    return left >= string.size;
}

function void
Append(StringContainer *container, String string)
{
    size_t left = container->capacity - container->size;
    size_t to_copy = string.size;
    if (to_copy > left) to_copy = left;

    CopyArray(to_copy, string.data, container->data + container->size);
    container->size += to_copy;
}

function void
Append(StringContainer *container, uint8_t character)
{
    if (container->size < container->capacity)
    {
        container->data[container->size++] = character;
    }
}

function void
AppendFill(StringContainer *container, size_t count, uint8_t character)
{
    size_t left = container->capacity - container->size;
    if (count > left) count = left;

    uint8_t *at = container->data + container->size;
    for (size_t i = 0; i < count; i += 1)
    {
        *at++ = character;
    }
    container->size += count;
}

function bool
CanFit(StringContainer *container, String string)
{
    return string.size <= container->capacity;
}

function void
Replace(StringContainer *container, String string)
{
    size_t size = string.size;
    if (size > container->capacity) size = container->capacity;
    if (string.data != container->data)
    {
        CopyArray(size, string.data, container->data);
    }
    container->size = size;
}

function void
Clear(StringContainer *container)
{
    container->size = 0;
}

//
// StringList
//

function bool
IsEmpty(StringList *list)
{
    bool result = !list->first;
    return result;
}

function StringNode
MakeStringNode(String string)
{
    StringNode result = {};
    result.string = string;
    return result;
}

function void
AddNode(StringList *list, StringNode *node)
{
    SllQueuePush(list->first, list->last, node);

    list->node_count += 1;
    list->total_size += node->string.size;
}

function void
PushStringNoCopy(StringList *list, Arena *arena, String string)
{
    StringNode *node = PushStruct(arena, StringNode);
    node->string = string;
    AddNode(list, node);
}

function void
PushString(StringList *list, Arena *arena, String string)
{
    StringNode *node = PushStruct(arena, StringNode);
    node->string = PushString(arena, string);
    AddNode(list, node);
}

function void
PushStringFV(StringList *list, Arena *arena, const char *fmt, va_list args)
{
    StringNode *node = PushStruct(arena, StringNode);
    node->string = PushStringFV(arena, fmt, args);
    AddNode(list, node);
}

function void
PushStringF(StringList *list, Arena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PushStringFV(list, arena, fmt, args);
    va_end(args);
}

function void
PushTempStringF(StringList *list, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PushStringFV(list, platform->GetTempArena(), fmt, args);
    va_end(args);
}

function void
InsertSeparators(StringList *list, Arena *arena, String separator, StringSeparatorFlags flags = 0)
{
    // I hate inserting in the middle of linked lists with tail pointers,
    // maybe that's just because I've never thought about how to really do
    // it properly, but whatever. We'll just make a new list with the same
    // nodes. Inefficient? Yeah!!!! Whatever!!!!!

    bool separator_before_first = !!(flags & StringSeparator_BeforeFirst);
    bool separator_after_last = !!(flags & StringSeparator_AfterLast);

    StringList new_list = {};

    if (separator_before_first)
    {
        PushString(&new_list, arena, separator);
    }

    for (StringNode *node = list->first;
         node;
         )
    {
        StringNode *next_node = node->next;

        AddNode(&new_list, node);
        if (next_node)
        {
            PushString(&new_list, arena, separator);
        }

        node = next_node;
    }

    if (separator_after_last)
    {
        PushString(&new_list, arena, separator);
    }

    *list = new_list;
}

function String
PushFlattenedString(StringList *list, Arena *arena, String separator = {}, StringSeparatorFlags flags = 0)
{
    size_t total_size = list->total_size;

    bool separator_before_first = !!(flags & StringSeparator_BeforeFirst);
    bool separator_after_last = !!(flags & StringSeparator_AfterLast);

    size_t separator_count = 0;
    if (list->node_count > 0)
    {
        separator_count = list->node_count - 1;
        if (separator_before_first) separator_count += 1;
        if (separator_after_last) separator_count += 1;
    }

    if (separator_count > 0)
    {
        total_size += separator_count*separator.size;
    }

    String result = {};
    result.size = total_size;
    result.data = PushArray(arena, result.size + 1, uint8_t);

    auto Append = [](uint8_t **dest, String string)
    {
        CopySize(string.size, string.data, *dest);
        *dest += string.size;
    };

    uint8_t *dest = result.data;
    if (separator_before_first)
    {
        Append(&dest, separator);
    }
    
    for (StringNode *node = list->first;
         node;
         node = node->next)
    {
        Append(&dest, node->string);
        if (node->next)
        {
            Append(&dest, separator);
        }
    }

    if (separator_after_last)
    {
        Append(&dest, separator);
    }

    result.data[result.size] = 0;

    Assert(dest == (result.data + result.size));
    return result;
}

function String
PushFlattenedTempString(StringList *list, String separator = {}, StringSeparatorFlags flags = 0)
{
    return PushFlattenedString(list, platform->GetTempArena(), separator, flags);
}

function String
Substring(String str, size_t start, size_t end = SIZE_MAX)
{
    start = Min(str.size, start);
    end   = Min(str.size, end);
    return MakeString(end - start, str.data + start);
}
