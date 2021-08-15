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

function Range
UndoOnce(View *view)
{
    Range result = MakeRange(-1, -1);

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;

    BufferBeginBulkEdit(buffer);

    UndoNode *node = undo->at;
    while (node->parent)
    {
        undo->depth -= 1;
        undo->at = node->parent;

        Range remove_range = MakeRangeStartLength(node->pos, node->forward.size);
        BufferReplaceRangeNoUndoHistory(buffer, remove_range, node->backward);

        result = MakeRange(node->pos, Max(node->pos, node->pos + node->backward.size));

        if (node->parent->ordinal == node->ordinal)
        {
            node = node->parent;
        }
        else
        {
            break;
        }
    }

    BufferEndBulkEdit(buffer);

    return result;
}

function Range
RedoOnce(View *view)
{
    Range result = MakeRange(-1, -1);

    Buffer *buffer = GetBuffer(view);
    auto undo = &buffer->undo;
    
    BufferBeginBulkEdit(buffer);

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

    BufferEndBulkEdit(buffer);

    return result;
}

function void
JumpToLocation(View *view, Jump jump)
{
    view->next_buffer = jump.buffer;
    Cursor *cursor = GetCursor(view->id, jump.buffer);
    SetCursor(cursor, jump.pos);
}
