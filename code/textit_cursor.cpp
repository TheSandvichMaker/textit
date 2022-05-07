function Cursor **
GetCursorSlot(ViewID view, BufferID buffer, bool make_if_missing)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value).u64[0];
    uint64_t slot = hash % ArrayCount(cursor_table->cursor_hash);

    CursorHashEntry *entry = cursor_table->cursor_hash[slot];

    while (entry &&
           (entry->key.value != key.value))
    {
        entry = entry->next_in_hash;
    }

    if (!entry && make_if_missing)
    {
        if (!cursor_table->first_free_cursor_hash_entry)
        {
            cursor_table->first_free_cursor_hash_entry = PushStructNoClear(&editor->transient_arena, CursorHashEntry);
            cursor_table->first_free_cursor_hash_entry->next_in_hash = nullptr;
        }
        entry = cursor_table->first_free_cursor_hash_entry;
        cursor_table->first_free_cursor_hash_entry = entry->next_in_hash;

        entry->next_in_hash = cursor_table->cursor_hash[slot];
        cursor_table->cursor_hash[slot] = entry;

        entry->key = key;

        if (!cursor_table->first_free_cursor)
        {
            cursor_table->first_free_cursor = PushStructNoClear(&editor->transient_arena, Cursor);
            cursor_table->first_free_cursor->next = nullptr;
        }
        entry->cursor = SllStackPop(cursor_table->first_free_cursor);
        ZeroStruct(entry->cursor);
    }

    return entry ? &entry->cursor : nullptr;
}

function Cursor *
CreateCursor(View *view)
{
    Buffer *buffer = GetBuffer(view);

    if (!cursor_table->first_free_cursor)
    {
        cursor_table->first_free_cursor = PushStructNoClear(&editor->transient_arena, Cursor);
        cursor_table->first_free_cursor->next = nullptr;
    }
    Cursor *result = SllStackPop(cursor_table->first_free_cursor);
    ZeroStruct(result);

    Cursor **slot = GetCursorSlot(view->id, buffer->id, true);
    result->next = *slot;
    *slot = result;

    return result;
}

function void
FreeCursor(Cursor *cursor)
{
    cursor->next = cursor_table->first_free_cursor;
    cursor_table->first_free_cursor = cursor;
}

function Cursor *
GetCursor(ViewID view, BufferID buffer)
{
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
    return GetCursor(view->id, (buffer ? buffer->id : view->buffer));
}

function Cursors
GetCursors(Arena *arena, View *view, Buffer *buffer)
{
    Cursor *cursor = GetCursor(view, buffer);

    size_t count = 0;
    for (Cursor *c = cursor; c; c = c->next) count++;

    Cursors result = {};
    result.cursors = PushArray(arena, count, Cursor *);

    for (Cursor *c = cursor; c; c = c->next)
    {
        result.cursors[result.count++] = c;
    }

    Sort(result.count, result.cursors, +[](Cursor *const &a, Cursor *const &b) {
        return a->pos < b->pos;
    });

    return result;
}

function void
DestroyCursors(ViewID view, BufferID buffer)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value).u64[0];
    uint64_t slot = hash % ArrayCount(cursor_table->cursor_hash);

    CursorHashEntry *entry = cursor_table->cursor_hash[slot];
    while (entry)
    {
        cursor_table->cursor_hash[slot] = entry->next_in_hash;

        entry->next_free = cursor_table->first_free_cursor_hash_entry;
        cursor_table->first_free_cursor_hash_entry = entry;

        entry = cursor_table->cursor_hash[slot];
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

function void
OrganizeCursorsAfterEdit(View *view, Buffer *buffer)
{
    (void)view;
    (void)buffer;
#if 0
    ScopedMemory temp;
    Cursors cursors = GetCursors(temp, view, buffer);

    // Merge overlapping cursors
    size_t dst = 0;
    size_t src = 1;
    while (src < cursors.count)
    {
        Cursor *a = cursors[dst];
        Cursor *b = cursors[src];

        if (RangesOverlap(a->selection.inner, b->selection.inner))
        {
            a->selection.inner = Union(a->selection.inner, b->selection.inner);
            a->selection.outer = Union(a->selection.outer, b->selection.outer);
        }
        else
        {
            dst++;
        }
        src++;
    }
    size_t count = dst + 1;

    Cursor **slot = GetCursorSlot(view->id, buffer->id, false);

    // clear cursors
    Cursor **at = slot;
    while (*at)
    {
        *at = (*at)->next;
    }
#endif
}
