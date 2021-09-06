function Cursor **
GetCursorSlot(ViewID view, BufferID buffer, bool make_if_missing)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value).u64[0];
    uint64_t slot = hash % ArrayCount(editor->cursor_hash);

    CursorHashEntry *entry = editor->cursor_hash[slot];

    while (entry &&
           (entry->key.value != key.value))
    {
        entry = entry->next_in_hash;
    }

    if (!entry && make_if_missing)
    {
        if (!editor->first_free_cursor_hash_entry)
        {
            editor->first_free_cursor_hash_entry = PushStructNoClear(&editor->transient_arena, CursorHashEntry);
            editor->first_free_cursor_hash_entry->next_in_hash = nullptr;
        }
        entry = editor->first_free_cursor_hash_entry;
        editor->first_free_cursor_hash_entry = entry->next_in_hash;

        entry->next_in_hash = editor->cursor_hash[slot];
        editor->cursor_hash[slot] = entry;

        entry->key = key;

        if (!editor->first_free_cursor)
        {
            editor->first_free_cursor = PushStructNoClear(&editor->transient_arena, Cursor);
            editor->first_free_cursor->next = nullptr;
        }
        entry->cursor = SllStackPop(editor->first_free_cursor);
        ZeroStruct(entry->cursor);
    }

    return entry ? &entry->cursor : nullptr;
}

function Cursor *
CreateCursor(View *view)
{
    Buffer *buffer = GetBuffer(view);

    if (!editor->first_free_cursor)
    {
        editor->first_free_cursor = PushStructNoClear(&editor->transient_arena, Cursor);
        editor->first_free_cursor->next = nullptr;
    }
    Cursor *result = SllStackPop(editor->first_free_cursor);
    ZeroStruct(result);

    Cursor **slot = GetCursorSlot(view->id, buffer->id, true);
    result->next = *slot;
    *slot = result;

    return result;
}

function void
FreeCursor(Cursor *cursor)
{
    cursor->next = editor->first_free_cursor;
    editor->first_free_cursor = cursor;
}

function Cursor *
GetCursor(ViewID view, BufferID buffer)
{
    if (editor->override_cursor)
    {
        return editor->override_cursor;
    }
    return *GetCursorSlot(view, buffer, true);
}

function Cursor *
IterateCursors(View *view)
{
    Buffer *buffer = GetBuffer(view);
    return GetCursor(view->id, buffer->id);
}

function Cursor *
IterateCursors(ViewID view, BufferID buffer)
{
    Cursor **cursor_slot = GetCursorSlot(view, buffer, false);
    return cursor_slot ? *cursor_slot : nullptr;
}

function Cursor *
GetCursor(View *view, Buffer *buffer)
{
    if (editor->override_cursor)
    {
        return editor->override_cursor;
    }
    return GetCursor(view->id, (buffer ? buffer->id : view->buffer));
}

function void
DestroyCursors(ViewID view, BufferID buffer)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value).u64[0];
    uint64_t slot = hash % ArrayCount(editor->cursor_hash);

    CursorHashEntry *entry = editor->cursor_hash[slot];
    while (entry)
    {
        editor->cursor_hash[slot] = entry->next_in_hash;

        entry->next_free = editor->first_free_cursor_hash_entry;
        editor->first_free_cursor_hash_entry = entry;

        entry = editor->cursor_hash[slot];
    }
}

function void
SetCursor(Cursor *cursor, int64_t pos, Range inner, Range outer)
{
    if (inner.start == 0 &&
        inner.end   == 0)
    {
        inner = MakeRange(pos);
    }
    if (outer.start == 0 &&
        outer.end   == 0)
    {
        outer = inner;
    }
    cursor->pos             = pos;
    cursor->selection.inner = inner;
    cursor->selection.outer = outer;
}

function void
SetSelection(Cursor *cursor, Range inner, Range outer)
{
    if (outer.start == 0 &&
        outer.end   == 0)
    {
        outer = inner;
    }
    cursor->selection.inner = inner;
    cursor->selection.outer = outer;
}
