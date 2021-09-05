function Buffer *
GetBuffer(View *view)
{
    return GetBuffer(view->buffer);
}

function void
SetCursor(View *view, int64_t pos, Range inner = {}, Range outer = {})
{
    Cursor *cursor = GetCursor(view);
    SetCursor(cursor, pos, inner, outer);
}

function void
MoveCursorRelative(View *view, V2i delta)
{
    int64_t pos = CalculateRelativeMove(GetBuffer(view), GetCursor(view), delta).pos;
    SetCursor(view, pos);
}

function void
SaveJump(View *view, BufferID buffer_id, int64_t pos)
{
    Buffer *buffer = GetBuffer(buffer_id);
    int64_t this_line = GetLineNumber(buffer, pos);
    for (uint32_t i = OldestJumpIndex(view); i < view->jump_top; i += 1)
    {
        Jump *jump = GetJump(view, i);
        if (jump->buffer == buffer_id)
        {
            int64_t line = GetLineNumber(GetBuffer(jump->buffer), jump->pos);
            if (line == this_line)
            {
                for (uint32_t j = i + 1; j < view->jump_top; j += 1)
                {
                    Jump *src = GetJump(view, j);
                    Jump *dst = GetJump(view, j - 1);
                    *dst = *src;
                }
                if (view->jump_top > 0) view->jump_top -= 1;
                if (view->jump_at >= view->jump_top) view->jump_at = view->jump_top;
            }
        }
    }
    Jump *jump = &view->jump_buffer[view->jump_at++ % ArrayCount(view->jump_buffer)];
    jump->buffer = buffer_id;
    jump->pos    = pos;
    view->jump_top = view->jump_at;
}

function Jump *
NextJump(View *view)
{
    Jump *result = nullptr;

    if (view->jump_at + 1 < view->jump_top)
    {
        view->jump_at += 1;
    }

    if (view->jump_at < view->jump_top)
    {
        result = &view->jump_buffer[view->jump_at % ArrayCount(view->jump_buffer)];
    }

    return result;
}

function Jump
PreviousJump(View *view)
{
    uint32_t min_index = OldestJumpIndex(view);

    if (view->jump_at <= min_index)
    {
        view->jump_at = min_index;
    }
    else
    {
        view->jump_at -= 1;
    }

    Jump result = view->jump_buffer[view->jump_at % ArrayCount(view->jump_buffer)];
    return result;
}

function Range
UndoOnce(View *view)
{
    Range result = MakeRange(-1, -1);

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;

    UndoNode *node = undo->at;
    while (node->parent)
    {
        undo->depth -= 1;
        undo->at = node->parent;

        Range remove_range = MakeRangeStartLength(node->pos, node->forward.size);
        BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->backward);

        result = MakeRange(node->pos, Max(node->pos, node->pos + node->backward.size));

        buffer->undo.current_ordinal = node->ordinal;

        if (node->parent->ordinal == node->ordinal)
        {
            node = node->parent;
        }
        else
        {
            break;
        }
    }

    return result;
}

function Range
RedoOnce(View *view)
{
    Range result = MakeRange(-1, -1);

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;
    
    UndoNode *node = NextChild(undo->at);
    while (node)
    {
        undo->depth += 1;
        undo->at = node;

        Range remove_range = MakeRangeStartLength(node->pos, node->backward.size);
        BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->forward);

        result = MakeRange(node->pos, Max(node->pos, node->pos + node->forward.size));

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

    return result;
}

function void
JumpToLocation(View *view, BufferID buffer, int64_t pos)
{
    view->next_buffer = buffer;
    Cursor *cursor = GetCursor(view->id, buffer);
    SetCursor(cursor, pos);
}
