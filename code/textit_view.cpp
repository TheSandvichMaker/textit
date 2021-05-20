static inline View *
NewView(Buffer *buffer)
{
    View *result = BootstrapPushStruct(View, arena);
    result->buffer = buffer;
    platform->PushTickEvent(); // tick at least once to initially render the view
    return result;
}

static inline void
DestroyView(View *view)
{
    Release(&view->arena);
}

static inline BufferLocation
ViewCursorToBufferLocation(Buffer *buffer, V2i cursor)
{
    if (cursor.y < 0) cursor.y = 0;

    int64_t at_pos     = 0;
    int64_t at_line    = 0;
    int64_t line_start = 0;
    int64_t line_end   = 0;
    while ((at_pos < buffer->count) &&
           (at_line < cursor.y))
    {
        int64_t newline_length = PeekNewline(buffer, at_pos);
        if (newline_length)
        {
            at_pos  += newline_length;
            at_line += 1;
            line_start = at_pos;
        }
        else
        {
            at_pos += 1;
        }
    }
    line_end = line_start;
    while ((line_end < buffer->count) &&
           !PeekNewline(buffer, line_end))
    {
        line_end += 1;
    }

    BufferLocation result = {};
    result.line_number = at_line;
    result.line_range = MakeRange(line_start, line_end);
    result.pos = ClampToRange(line_start + cursor.x, BufferRange(buffer));
    if (result.pos > result.line_range.end)
    {
        result.pos = result.line_range.end;
    }
    return result;
}

static inline V2i
BufferPosToViewCursor(Buffer *buffer, int64_t pos)
{
    if (pos < 0) pos = 0; // TODO: Should maybe assert in debug
    if (pos > buffer->count) pos = buffer->count;

    int64_t at_pos  = 0;
    int64_t at_line = 0;
    int64_t at_col  = 0;
    while (at_pos < buffer->count && at_pos < pos)
    {
        int64_t newline_length = PeekNewline(buffer, at_pos);
        if (newline_length)
        {
            at_pos += newline_length;
            at_col = 0;
            at_line += 1;
        }
        else
        {
            at_pos += 1;
            at_col += 1;
        }
    }

    V2i result = MakeV2i(at_col, at_line);
    return result;
}

static inline void
SetCursorPos(View *view, int64_t pos)
{
    view->cursor = BufferPosToViewCursor(view->buffer, pos);
}

static inline void
MoveCursorRelative(View *view, V2i delta)
{
    V2i old_cursor = view->cursor;
    BufferLocation loc = ViewCursorToBufferLocation(view->buffer, old_cursor + delta);

    int64_t offset = 0;
    while (IsInBufferRange(view->buffer, loc.pos + offset) &&
           IsTrailingUtf8Byte(ReadBufferByte(view->buffer, loc.pos + offset)))
    {
        if (delta.x > 0)
        {
            offset += 1;
        }
        else
        {
            offset -= 1;
        }
    }
    loc.pos += offset;
    delta.x += offset;

    V2i new_cursor = view->cursor + delta;

    int64_t line_length = loc.line_range.end - loc.line_range.start;
    new_cursor.y = ClampToRange(new_cursor.y, MakeRange(0, loc.line_number));

    if (delta.x != 0)
    {
        int64_t old_x = ClampToRange(old_cursor.x, MakeRange(0, line_length));
        int64_t new_x = ClampToRange(old_x + delta.x, MakeRange(0, line_length));
        if (new_x != old_x)
        {
            view->cursor.x = new_x;
        }
    }
    view->cursor.y = new_cursor.y;
}

static inline void
WriteText(View *view, String text)
{
    Buffer *buffer = view->buffer;
    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    uint64_t start_ordinal = GetCurrentUndoOrdinal(buffer);
    bool should_merge = false;
    for (size_t i = 0; i < text.size; i += 1)
    {
        if (text.data[0] != '\n')
        {
            UndoNode *last_undo = GetCurrentUndoNode(buffer);

            String last_text = last_undo->forward;
            if ((last_undo->pos == loc.pos - 1) &&
                (last_text.size > 0))
            {
                if ((text.data[0] == ' ') &&
                    (last_text.data[0] == ' '))
                {
                    should_merge = true;
                }
                else if (IsValidIdentifierAscii(text.data[0]) &&
                         IsValidIdentifierAscii(last_text.data[0]))
                {
                    should_merge = true;
                }
            }
        }
    }
    if (text.data[0] == '\n' || IsPrintableAscii(text.data[0]) || IsUtf8Byte(text.data[0]))
    {
        int64_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), text);
        SetCursorPos(view, pos);
    }
    if (should_merge)
    {
        uint64_t end_ordinal = GetCurrentUndoOrdinal(buffer);
        MergeUndoHistory(buffer, start_ordinal, end_ordinal);
    }
}
