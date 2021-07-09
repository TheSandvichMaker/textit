function Buffer *
GetBuffer(View *view)
{
    return GetBuffer(view->buffer);
}

function void
SetCursor(View *view, int64_t pos, int64_t mark = -1)
{
    // TODO: What to do with this function

    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = ClampToBufferRange(buffer, pos);
    if (mark >= 0)
    {
        cursor->selection = MakeRange(ClampToBufferRange(buffer, mark), pos);
    }
}

function void
MoveCursorRelative(View *view, V2i delta)
{
    Cursor *cursor = GetCursor(view);
    cursor->pos = CalculateRelativeMove(GetBuffer(view), cursor->pos, delta);
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
