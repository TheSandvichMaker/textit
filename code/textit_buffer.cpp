static inline LineEndKind
GuessLineEndKind(String string)
{
    int64_t lf   = 0;
    int64_t crlf = 0;
    for (size_t i = 0; i < string.size;)
    {
        if (string.data[i + 0] == '\r' &&
            string.data[i + 1] == '\n')
        {
            crlf += 1;
            i += 2;
        }
        else if (string.data[i] == '\n')
        {
            lf += 1;
            i += 1;
        }
        else
        {
            i += 1;
        }
    }

    LineEndKind result = LineEnd_LF;
    if (crlf > lf)
    {
        result = LineEnd_CRLF;
    }
    return result;
}

static inline Buffer *
NewBuffer(String buffer_name)
{
    Buffer *result = BootstrapPushStruct(Buffer, arena);
    result->name = PushString(&result->arena, buffer_name);
    return result;
}

static inline Buffer *
OpenFileIntoNewBuffer(String filename)
{
    Buffer *result = NewBuffer(filename);
    size_t file_size = platform->ReadFileInto(sizeof(result->text), result->text, filename);
    result->count = (int64_t)file_size;
    result->line_end = GuessLineEndKind(MakeString(result->count, (uint8_t *)result->text));
    return result;
}

static inline Range
BufferRange(Buffer *buffer)
{
    return MakeRange(0, buffer->count);
}

static inline uint8_t
Peek(Buffer *buffer, int64_t index)
{
    uint8_t result = 0;
    if ((index >= 0) &&
        (index < buffer->count))
    {
        result = buffer->text[index];
    }
    return result;
}

static inline int64_t
PeekNewline(Buffer *buffer, int64_t index)
{
    if (Peek(buffer, index + 0) == '\r' &&
        Peek(buffer, index + 1) == '\n')
    {
        return 2;
    }
    if (Peek(buffer, index) == '\n')
    {
        return 1;
    }
    return 0;
}

static inline int64_t
PeekNewlineBackward(Buffer *buffer, int64_t index)
{
    if (Peek(buffer, index - 0) == '\n' &&
        Peek(buffer, index - 1) == '\r')
    {
        return 2;
    }
    if (Peek(buffer, index) == '\n')
    {
        return 1;
    }
    return 0;
}

static inline int64_t
ScanWordForward(Buffer *buffer, int64_t pos)
{
    if (IsAlphanumericAscii(Peek(buffer, pos)))
    {
        while (IsAlphanumericAscii(Peek(buffer, pos)))
        {
            pos += 1;
        }
    }
    else
    {
        while (!IsAlphanumericAscii(Peek(buffer, pos)) && !IsWhitespaceAscii(Peek(buffer, pos)))
        {
            pos += 1;
        }
    }
    while (IsWhitespaceAscii(Peek(buffer, pos)))
    {
        pos += 1;
    }
    return pos;
}

static inline int64_t
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    while (IsWhitespaceAscii(Peek(buffer, pos - 1)))
    {
        pos -= 1;
    }
    if (IsAlphanumericAscii(Peek(buffer, pos - 1)))
    {
        while (IsAlphanumericAscii(Peek(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    else
    {
        while (!IsAlphanumericAscii(Peek(buffer, pos - 1)) && !IsWhitespaceAscii(Peek(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    return pos;
}

static inline int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = ClampRange(range, BufferRange(buffer));

    int64_t delta = range.start - range.end + (int64_t)text.size;

    if (delta > 0)
    {
        int64_t offset = delta;
        for (int64_t pos = buffer->count + delta; pos >= range.start + offset; pos -= 1)
        {
            buffer->text[pos] = buffer->text[pos - offset];
        }
    }
    else if (delta < 0)
    {
        int64_t offset = -delta;
        for (int64_t pos = range.start; pos < buffer->count - offset; pos += 1)
        {
            buffer->text[pos] = buffer->text[pos + offset];
        }
    }

    buffer->count += delta;

    for (size_t i = 0; i < text.size; i += 1)
    {
        buffer->text[range.start + i] = text.data[i];
    }

    int64_t edit_end = range.start;
    if (delta > 0)
    {
        edit_end += delta;
    }
    return edit_end;
}
