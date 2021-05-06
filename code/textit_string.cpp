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
