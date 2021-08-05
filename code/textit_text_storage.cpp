function void
AllocateTextStorage(TextStorage *storage, int64_t capacity)
{
    Assert(!storage->text);

    ZeroStruct(storage);
    storage->capacity = capacity;
    storage->text = (uint8_t *)platform->ReserveMemory((int64_t)storage->capacity, 0, LOCATION_STRING("text storage"));
}

function void
EnsureSpace(TextStorage *storage, int64_t append_size)
{
    Assert(storage->count + append_size <= storage->capacity);

    if (storage->count + append_size > storage->committed)
    {
        int64_t to_commit = AlignPow2(append_size, platform->page_size);
        platform->CommitMemory(storage->text + storage->committed, to_commit);
        storage->committed += to_commit;
        Assert(storage->committed >= (storage->count + append_size));
    }
}

function int64_t
TextStorageReplaceRange(TextStorage *storage, Range range, String text)
{
    range = ClampRange(range, MakeRange(0, storage->count));

    PlatformHighResTime start = platform->GetTime();

    int64_t delta = range.start - range.end + (int64_t)text.size;
    EnsureSpace(storage, delta);
    // TODO: Decommit behaviour

    int64_t total_moved = 0;
    if (delta != 0)
    {
        uint8_t *source = (delta > 0
                           ? storage->text + range.start
                           : storage->text + range.end);
        uint8_t *dest   = source + delta;
        uint8_t *end    = storage->text + storage->count;
        memmove(dest, source, end - source);

        total_moved = end - source;

        storage->count += delta;
    }
    memcpy(storage->text + range.start, text.data, text.size);

    int64_t edit_end = range.start;
    if (delta > 0)
    {
        edit_end += delta;
    }

    PlatformHighResTime end = platform->GetTime();
    if (total_moved != 0 || text.size != 0)
    {
        double time_elapsed = platform->SecondsElapsed(start, end);
        platform->DebugPrint("moved %lld bytes, copied %zu bytes in %fms\n", total_moved, text.size, time_elapsed*1000.0);
    }

    return edit_end;
}
