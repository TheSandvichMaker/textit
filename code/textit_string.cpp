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
    if ((b & 0xC0) == 0x80)
    {
        return true;
    }
    else
    {
        return false;
    }
}

function bool
IsUtf8Byte(uint8_t b)
{
    return (IsHeadUtf8Byte(b) ||
            IsTrailingUtf8Byte(b));
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

function bool
MatchPrefix(String string, String prefix)
{
    if (prefix.size > string.size) return false;
    if (string.size > prefix.size) string.size = prefix.size;
    return AreEqual(string, prefix);
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
