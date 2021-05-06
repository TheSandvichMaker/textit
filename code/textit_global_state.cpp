static inline void *
CreateGlobalState_(size_t size, size_t align, void *variable_at, void *prototype, String guid)
{
    SimpleAssert(global_state->entry_count < MAX_GLOBAL_STATE);

    GlobalStateEntry *entry = &global_state->entries[global_state->entry_count++];
    entry->guid = guid;
    entry->variable_at = (void **)variable_at;
    entry->prototype = prototype;
    entry->data_size = size;
    entry->data_align = align;

    return nullptr;
}

static GlobalStateEntry *
FindEntry(GlobalState *state, String guid)
{
    GlobalStateEntry *result = nullptr;
    for (size_t i = 0; i < state->entry_count; ++i)
    {
        GlobalStateEntry *entry = &state->entries[i];
        if (AreEqual(entry->guid, guid))
        {
            result = entry;
            break;
        }
    }
    return result;
}

static void
RestoreGlobalState(GlobalState *previous_global_state, Arena *permanent_storage)
{
    if (!previous_global_state)
    {
        previous_global_state = PushStruct(permanent_storage, GlobalState);
    }

    for (size_t i = 0; i < global_state->entry_count; ++i)
    {
        GlobalStateEntry *entry = &global_state->entries[i];
        GlobalStateEntry *previous_entry = FindEntry(previous_global_state, entry->guid);
        if (previous_entry)
        {
            entry->data = previous_entry->data;
        }
        else
        {
            entry->data = PushAlignedSize(permanent_storage, entry->data_size, entry->data_align);
            CopySize(entry->data_size, entry->prototype, entry->data);
        }
        *entry->variable_at = entry->data;
    }

    CopyStruct(global_state, previous_global_state);
}
