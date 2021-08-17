function Tag *
AllocateTag()
{
    if (!editor->first_free_tag)
    {
        editor->first_free_tag = PushStructNoClear(&editor->transient_arena, Tag);
        editor->first_free_tag->next_free = nullptr;
    }
    Tag *result = editor->first_free_tag;
    editor->first_free_tag = result->next_free;

    ZeroStruct(result);

    return result;
}

function void
FreeTag(Tag *tag)
{
    tag->next = editor->first_free_tag;
    editor->first_free_tag = tag;
}

function Tag **
GetTagSlot(Project *project, Tag *tag)
{
    Tag **result = &project->tag_table[tag->hash.u32[0] % ArrayCount(project->tag_table)];
    while (*result && *result != tag)
    {
        result = &(*result)->next_in_hash;
    }
    return result;
}

function Tag *
AddTag(Tags *tags, Buffer *buffer, String name)
{
    Project *project = buffer->project;

    Tag *result = AllocateTag();

    HashResult hash = HashString(name);
    result->hash = hash;

    Tag **slot = &project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
    result->next_in_hash = *slot;
    *slot = result;

    DllInsertBack(&tags->sentinel, result);

    result->buffer = buffer->id;

    return result;
}

function void
FreeAllTags(Buffer *buffer)
{
    Project *project = buffer->project;

    Tags *tags = buffer->tags;
    while (DllIsntEmpty(&tags->sentinel))
    {
        Tag *tag = tags->sentinel.next;

        Tag **slot = GetTagSlot(project, tag);
        Assert(*slot == tag);
        *slot = tag->next_in_hash;

        DllRemove(tag);
        FreeTag(tag);
    }
}

function Tag *
FindTag(Project *project, String name)
{
    HashResult hash = HashString(name);

    Tag *result = project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
    while (result)
    {
        if (result->hash == hash)
        {
            Buffer *buffer = GetBuffer(result->buffer);

            ScopedMemory temp(platform->GetTempArena());
            String tag_name = PushBufferRange(temp, buffer, MakeRangeStartLength(result->pos, result->length));
            if (AreEqual(name, tag_name))
            {
                break;
            }
        }

        result = result->next_in_hash;
    }

    return result;
}

function void
ParseTags(Buffer *buffer)
{
    LanguageSpec *lang = buffer->language;
    if (lang->ParseTags)
    {
        lang->ParseTags(buffer);
    }
}
