static inline void
PushUndo(Buffer *buffer, int64_t pos, String forward, String backward)
{
    auto undo = &buffer->undo;
    UndoNode *node = PushStruct(&buffer->arena, UndoNode);

    node->parent = undo->at;
    node->next_child = node->parent->first_child;
    node->parent->first_child = node;

    undo->current_ordinal += 1;
    node->ordinal = undo->current_ordinal;

    node->pos = pos;
    node->forward = forward;
    node->backward = backward;

    undo->at = node;
    undo->depth += 1;
}

static inline UndoNode *
CurrentUndoNode(Buffer *buffer)
{
    auto undo = &buffer->undo;
    return undo->at;
}

static inline void
SelectNextUndoBranch(Buffer *buffer)
{
    UndoNode *node = CurrentUndoNode(buffer);

    uint32_t child_count = 0;
    for (UndoNode *child = node->first_child;
         child;
         child = child->next_child)
    {
        child_count += 1;
    }

    node->selected_branch = (node->selected_branch + 1) % child_count;
}

static inline UndoNode *
NextChild(UndoNode *node)
{
    UndoNode *result = node->first_child;
    for (uint32_t i = 0; i < node->selected_branch; i += 1)
    {
        result = result->next_child;
    }
    return result;
}

static inline uint64_t
CurrentUndoOrdinal(Buffer *buffer)
{
    return buffer->undo.current_ordinal;
}

static inline void
MergeUndoHistory(Buffer *buffer, uint64_t first_ordinal, uint64_t last_ordinal)
{
    Assert(last_ordinal >= first_ordinal);
    UndoNode *node = buffer->undo.at;
    while (node && node->ordinal > last_ordinal)
    {
        node = node->parent;
    }
    while (node && node->ordinal > first_ordinal)
    {
        node->ordinal = first_ordinal;
        node = node->parent;
    }
    if (buffer->undo.current_ordinal == last_ordinal)
    {
        buffer->undo.current_ordinal = first_ordinal;
    }
}

static inline bool
IsInBufferRange(Buffer *buffer, int64_t pos)
{
    bool result = ((pos >= 0) && (pos < buffer->count));
    return result;
}

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
    result->undo.at = &result->undo.root;
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
ReadBufferByte(Buffer *buffer, int64_t pos)
{
    uint8_t result = 0;
    if (IsInBufferRange(buffer, pos))
    {
        result = buffer->text[pos];
    }
    return result;
}

static inline int64_t
PeekNewline(Buffer *buffer, int64_t pos)
{
    if (ReadBufferByte(buffer, pos + 0) == '\r' &&
        ReadBufferByte(buffer, pos + 1) == '\n')
    {
        return 2;
    }
    if (ReadBufferByte(buffer, pos) == '\n')
    {
        return 1;
    }
    return 0;
}

static inline int64_t
PeekNewlineBackward(Buffer *buffer, int64_t pos)
{
    if (ReadBufferByte(buffer, pos - 0) == '\n' &&
        ReadBufferByte(buffer, pos - 1) == '\r')
    {
        return 2;
    }
    if (ReadBufferByte(buffer, pos) == '\n')
    {
        return 1;
    }
    return 0;
}

static inline int64_t
ScanWordEndForward(Buffer *buffer, int64_t pos)
{
    bool skipped_whitespace = false;
    if (IsInBufferRange(buffer, pos) &&
        IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        skipped_whitespace = true;
        while (IsInBufferRange(buffer, pos) &&
               IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
            if (ReadBufferByte(buffer, pos) == '\n')
            {
                return pos;
            }
        }
    }
    if (IsInBufferRange(buffer, pos) &&
        IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
    {
        while (IsInBufferRange(buffer, pos) && IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    else if (!skipped_whitespace)
    {
        while (IsInBufferRange(buffer, pos) &&
               !IsAlphanumericAscii(ReadBufferByte(buffer, pos)) && !IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    return pos;
}

static inline int64_t
ScanWordForward(Buffer *buffer, int64_t pos)
{
    if (IsInBufferRange(buffer, pos) &&
        IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
    {
        while (IsInBufferRange(buffer, pos) &&
               IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    else
    {
        while (IsInBufferRange(buffer, pos) &&
               !IsAlphanumericAscii(ReadBufferByte(buffer, pos)) && !IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos += 1;
    }
    return pos;
}

static inline int64_t
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos - 1)))
    {
        pos -= 1;
    }
    if (IsInBufferRange(buffer, pos) &&
        IsAlphanumericAscii(ReadBufferByte(buffer, pos - 1)))
    {
        while (IsInBufferRange(buffer, pos) &&
               IsAlphanumericAscii(ReadBufferByte(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    else
    {
        while (IsInBufferRange(buffer, pos) &&
               !IsAlphanumericAscii(ReadBufferByte(buffer, pos - 1)) && !IsWhitespaceAscii(ReadBufferByte(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    return pos;
}

static inline String
BufferPushRange(Arena *arena, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    String result = PushString(arena, string);
    return result;
}

static inline int64_t
BufferReplaceRangeNoUndoHistory(Buffer *buffer, Range range, String text)
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

static inline int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = ClampRange(range, BufferRange(buffer));

    String backward = {};
    if (RangeSize(range) > 0)
    {
        backward = BufferPushRange(&buffer->arena, buffer, range);
    }

    String forward = {};
    if (text.size > 0)
    {
        forward = PushString(&buffer->arena, text);
    }

    PushUndo(buffer, range.start, forward, backward);

    int64_t result = BufferReplaceRangeNoUndoHistory(buffer, range, text);
    return result;
}

static inline int64_t
UndoOnce(Buffer *buffer)
{
    int64_t pos = -1;

    auto undo = &buffer->undo;

    UndoNode *node = undo->at;
    while (node->parent)
    {
        undo->depth -= 1;
        undo->at = node->parent;

        Range remove_range = MakeRangeStartLength(node->pos, node->forward.size);
        pos = BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->backward);

        if (node->parent->ordinal == node->ordinal)
        {
            node = node->parent;
        }
        else
        {
            break;
        }
    }
    
    return pos;
}

static inline int64_t
RedoOnce(Buffer *buffer)
{
    int64_t pos = -1;

    auto undo = &buffer->undo;
    
    UndoNode *node = NextChild(undo->at);
    while (node)
    {
        undo->depth += 1;
        undo->at = node;

        Range remove_range = MakeRangeStartLength(node->pos, node->backward.size);
        pos = BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->forward);

        UndoNode *next_node = NextChild(node);
        if (next_node && next_node->ordinal == node->ordinal)
        {
            node = next_node;
        }
        else
        {
            break;
        }
    }

    return pos;
}
