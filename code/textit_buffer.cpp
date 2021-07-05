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

static inline bool
IsInBufferRange(Buffer *buffer, int64_t pos)
{
    bool result = ((pos >= 0) && (pos < buffer->count));
    return result;
}

static inline int64_t
ClampToBufferRange(Buffer *buffer, int64_t pos)
{
    if (pos >= buffer->count) pos = buffer->count - 1;
    if (pos < 0) pos = 0;
    return pos;
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

static inline bool
AdvanceOverNewline(Buffer *buffer, int64_t *pos)
{
    int64_t length = PeekNewline(buffer, *pos);

    bool result = (length > 0);
    if (result)
    {
        *pos = *pos + length;
    }
    else
    {
        *pos = *pos + 1;
    }
    return result;
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

// taxonomy of a word motion

// #include 
// ^ cursor start
//  ^ selection start
//         ^ cursor/selection end

// ##include
// ^ cursor start
// ^ selection start
//  ^ cursor/selection end

static inline Range
ScanWordForward(Buffer *buffer, int64_t pos)
{
    if (PeekNewline(buffer, pos + 1))
    {
        return MakeRange(pos + 1 + PeekNewline(buffer, pos + 1));
    }

    CharacterClassFlags p0_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    CharacterClassFlags p1_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos + 1));
    if (p0_class != p1_class)
    {
        pos += 1;
    }

    Range result = MakeRange(pos);

    while (IsInBufferRange(buffer, pos + 1) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos + 1)) == p1_class)
    {
        pos += 1;
    }

    while (IsInBufferRange(buffer, pos + 1) &&
           IsHorizontalWhitespaceAscii(ReadBufferByte(buffer, pos + 1)))
    {
        pos += 1;
    }

    result.end = pos;
    return result;
}

static inline Range
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    if (PeekNewlineBackward(buffer, pos - 1))
    {
        pos -= PeekNewlineBackward(buffer, pos - 1);
        if (!PeekNewlineBackward(buffer, pos - 1))
        {
            pos -= 1;
        }
        return MakeRange(pos);
    }

    CharacterClassFlags p0_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    CharacterClassFlags p1_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos - 1));
    if (!HasFlag(p0_class, Character_Whitespace) && (p0_class != p1_class))
    {
        pos -= 1;
    }

    Range result = MakeRange(pos);

    while (IsInBufferRange(buffer, pos) &&
           IsHorizontalWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos -= 1;
    }

    CharacterClassFlags skip_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));

    while (IsInBufferRange(buffer, pos - 1) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos - 1)) == skip_class)
    {
        pos -= 1;
    }

    result.end = pos;
    return result;
}

static inline Range
EncloseLine(Buffer *buffer, int64_t pos, bool including_newline = false)
{
    Range result = MakeRange(pos, pos);
    while (IsInBufferRange(buffer, result.start - 1) && !PeekNewlineBackward(buffer, result.start - 1))
    {
        result.start -= 1;
    }

    int64_t newline_length = 0;
    while (IsInBufferRange(buffer, result.end + 1))
    {
        newline_length = PeekNewline(buffer, result.end + 1);
        if (newline_length)
        {
            if (including_newline)
            {
                result.end += newline_length;
            }
            break;
        }
        else
        {
            result.end += 1;
        }
    }
    return result;
}

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

static inline String
BufferPushRange(Arena *arena, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    String result = PushString(arena, string);
    return result;
}

static inline void
EnsureSpace(Buffer *buffer, int64_t append_size)
{
    Assert(buffer->count + append_size <= buffer->capacity);

    if (buffer->count + append_size > buffer->committed)
    {
        int64_t to_commit = AlignPow2(append_size, platform->page_size);
        platform->CommitMemory(buffer->text + buffer->committed, to_commit);
        buffer->committed += to_commit;
        Assert(buffer->committed >= (buffer->count + append_size));
    }
}

static inline int64_t
BufferReplaceRangeNoUndoHistory(Buffer *buffer, Range range, String text)
{
    if (buffer->flags & Buffer_ReadOnly)
    {
        return range.start;
    }

    range = ClampRange(range, BufferRange(buffer));

    int64_t delta = range.start - range.end + (int64_t)text.size;
    EnsureSpace(buffer, delta);
    // TODO: Decommit behaviour

    if (delta != 0)
    {
        uint8_t *source = (delta > 0
                           ? buffer->text + range.start
                           : buffer->text + range.end);
        uint8_t *dest   = source + delta;
        uint8_t *end    = buffer->text + buffer->count;
        memmove(dest, source, end - source);

        buffer->count += delta;
    }
    memcpy(buffer->text + range.start, text.data, text.size);

    int64_t edit_end = range.start;
    if (delta > 0)
    {
        edit_end += delta;
    }

    buffer->dirty = true;
    return edit_end;
}

static inline int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = SanitizeRange(range);
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

