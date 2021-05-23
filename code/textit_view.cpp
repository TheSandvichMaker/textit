static inline Buffer *
GetBuffer(View *view)
{
    return GetBuffer(view->buffer);
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
    pos = ClampToBufferRange(buffer, pos);

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
    Buffer *buffer = GetBuffer(view);

    pos = ClampToBufferRange(buffer, pos);
    buffer->cursor.pos = pos;
    view->cursor = BufferPosToViewCursor(buffer, pos);
}

static inline void
MoveCursorRelative(View *view, V2i delta)
{
    Buffer *buffer = GetBuffer(view);

    V2i old_cursor = view->cursor;
    BufferLocation loc = ViewCursorToBufferLocation(buffer, old_cursor + delta);

    int64_t offset = 0;
    while (IsInBufferRange(buffer, loc.pos + offset) &&
           IsTrailingUtf8Byte(ReadBufferByte(buffer, loc.pos + offset)))
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
    new_cursor.y = ClampToRange(new_cursor.y, MakeRange(0, loc.line_number + 1));

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

    SetCursorPos(buffer, loc.pos);
}
