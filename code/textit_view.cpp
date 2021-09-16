function View *
OpenNewView(BufferID buffer)
{
    ViewID id = {};
    if (editor->view_count < MAX_VIEW_COUNT)
    {
        id = editor->free_view_ids[MAX_VIEW_COUNT - editor->view_count - 1];
        id.generation += 1;

        editor->used_view_ids[editor->view_count] = id;
        editor->view_count += 1;
    }

    View *result = BootstrapPushStruct(View, arena);
    result->id     = id;
    result->buffer = buffer;

    result->line_hashes      = PushArray(&result->arena, 2048, uint64_t);
    result->prev_line_hashes = PushArray(&result->arena, 2048, uint64_t);

    editor->views[result->id.index] = result;

    return result;
}

function View *
GetView(ViewID id)
{
    View *result = editor->views[0];
    if (id.index > 0 && id.index < MAX_VIEW_COUNT)
    {
        View *view = editor->views[id.index];
        if (view && view->id == id)
        {
            result = view;
        }
    }
    return result;
}

function void
DestroyView(ViewID id)
{
    View *view = GetView(id);

    size_t used_id_index = 0;
    for (size_t i = 0; i < editor->view_count; i += 1)
    {
        ViewID test_id = editor->used_view_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor->free_view_ids[MAX_VIEW_COUNT - editor->view_count] = id;
    editor->used_view_ids[used_id_index] = editor->used_view_ids[--editor->view_count];

    for (size_t i = 0; i < editor->buffer_count; i += 1)
    {
        BufferID buffer_id = editor->used_buffer_ids[i];
        DestroyCursors(id, buffer_id);
    }

    Release(&view->arena);
    editor->views[id.index] = nullptr;
}

function ViewID
DuplicateView(ViewID id)
{
    ViewID result = ViewID::Null();

    View *view = GetView(id);
    if (view != editor->null_view)
    {
        View *new_view = OpenNewView(view->buffer);

        Cursor *cursor = GetCursor(view);
        Cursor *new_cursor = GetCursor(new_view);

        new_cursor->sticky_col = cursor->sticky_col;
        new_cursor->selection  = cursor->selection;
        new_cursor->pos        = cursor->pos;

        new_view->jump_at  = view->jump_at;
        new_view->jump_top = view->jump_top;
        CopyArray(ArrayCount(view->jump_buffer), view->jump_buffer, new_view->jump_buffer);

        new_view->scroll_at = view->scroll_at;
        result = new_view->id;
    }

    return result;
}

function ViewIterator
IterateViews(void)
{
    ViewIterator result = {};
    result.index = 1;
    result.view  = GetView(editor->used_view_ids[result.index]);
    return result;
}

function bool
IsValid(ViewIterator *iter)
{
    return (iter->index < editor->view_count);
}

function void
Next(ViewIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor->view_count)
    {
        iter->view = GetView(editor->used_view_ids[iter->index]);
    }
}

function View *
GetActiveView(void)
{
    return GetView(editor->active_window->view);
}

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
MoveCursorRelative(View *view, Cursor *cursor, V2i delta)
{
    int64_t pos = CalculateRelativeMove(GetBuffer(view), cursor, delta).pos;
    SetCursor(cursor, pos);
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

        buffer->undo.current_ordinal = node->ordinal + 1;

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
