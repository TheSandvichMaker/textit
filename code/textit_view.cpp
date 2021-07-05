function Buffer *
GetBuffer(View *view)
{
    return GetBuffer(view->buffer);
}

function BufferLocation
CalculateBufferLocationFromPos(Buffer *buffer, int64_t pos)
{
    // TESTME

    pos = ClampToBufferRange(buffer, pos);

    BufferLocation result = {};

    while (result.pos < pos)
    {
        if (AdvanceOverNewline(buffer, &result.pos))
        {
            result.line += 1;
            result.col   = 0;

            result.line_range.start = result.pos;
            result.line_range.end = result.pos;
        }
        else
        {
            result.col += 1;
        }
    }

    while (IsInBufferRange(buffer, result.line_range.end))
    {
        if (AdvanceOverNewline(buffer, &result.line_range.end))
        {
            break;
        }
    }

    return result;
}

function BufferLocation
CalculateBufferLocationFromLineCol(Buffer *buffer, int64_t line, int64_t col)
{
    // TESTME

    BufferLocation result = {};

    while (IsInBufferRange(buffer, result.pos) && (result.line < line))
    {
        if (AdvanceOverNewline(buffer, &result.pos))
        {
            result.line += 1;
            result.line_range.start = result.pos;
            result.line_range.end = result.pos;
        }
        else
        {
            result.pos += 1;
        }
    }

    while (IsInBufferRange(buffer, result.pos) && (result.col < col))
    {
        if (PeekNewline(buffer, result.pos))
        {
            break;
        }

        result.pos += 1;
        result.col += 1;
    }

    while (IsInBufferRange(buffer, result.line_range.end))
    {
        if (AdvanceOverNewline(buffer, &result.line_range.end))
        {
            break;
        }
    }

    return result;
}

function void
SetCursor(View *view, int64_t pos, int64_t mark = -1)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = ClampToBufferRange(buffer, pos);
    if (mark >= 0)
    {
        cursor->mark = ClampToBufferRange(buffer, mark);
    }
}

function void
MoveCursorRelative(View *view, V2i delta)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    BufferLocation curr_loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
    int64_t target_line = curr_loc.line + delta.y;
    int64_t target_col  = curr_loc.col  + delta.x;
    BufferLocation target_loc = CalculateBufferLocationFromLineCol(buffer, target_line, target_col);

    int64_t pos = target_loc.pos;

    while (IsInBufferRange(buffer, pos) &&
           IsTrailingUtf8Byte(ReadBufferByte(buffer, pos)))
    {
        pos += SignOf(delta.x);
    }

    SetCursor(view, pos);
}

function int64_t
UndoOnce(View *view)
{
    int64_t pos = -1;

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;

    UndoNode *node = undo->at;
    while (node->parent)
    {
        undo->depth -= 1;
        undo->at = node->parent;

        Range remove_range = MakeRangeStartLength(node->pos, node->forward.size);
        BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->backward);
        pos = node->pos;

        SetCursor(view, pos, pos + node->backward.size - 1);

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

function int64_t
RedoOnce(View *view)
{
    int64_t pos = -1;

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;
    
    UndoNode *node = NextChild(undo->at);
    while (node)
    {
        undo->depth += 1;
        undo->at = node;

        Range remove_range = MakeRangeStartLength(node->pos, node->backward.size);
        BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->forward);

        pos = node->pos;

        SetCursor(view, pos, pos + node->forward.size - 1);

        UndoNode *next_node = NextChild(node);
        if (next_node &&
            (next_node->ordinal == node->ordinal))
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
