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

function void
ParseCppTags(Buffer *buffer)
{
    PlatformHighResTime start = platform->GetTime();

    FreeAllTags(buffer);

    TokenIterator it = IterateTokens(buffer);
    while (IsValid(&it))
    {
        Token *t = Next(&it);
        if (!t) break;

        ScopedMemory temp(platform->GetTempArena());
        String text = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));

        if (AreEqual(text, "struct"_str) ||
            AreEqual(text, "enum"_str)   ||
            AreEqual(text, "class"_str)  ||
            AreEqual(text, "union"_str))
        {
            t = Next(&it);

            if (t->kind == Token_Identifier)
            {
                String name = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));
                Tag *tag = AddTag(buffer->tags, buffer, name);
                tag->kind   = Tag_Declaration;
                tag->pos    = t->pos;
                tag->length = t->length;
            }
        }

        if (AreEqual(text, "typedef"_str))
        {
            t = Next(&it);
            t = Next(&it);
            if (!t) break;

            if (t->kind == Token_Identifier)
            {
                String name = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));
                Tag *tag = AddTag(buffer->tags, buffer, name);
                tag->kind   = Tag_Declaration;
                tag->pos    = t->pos;
                tag->length = t->length;
            }
        }
    }

    PlatformHighResTime end = platform->GetTime();
    platform->DebugPrint("Tag parse time: %fms\n", 1000.0*platform->SecondsElapsed(start, end));
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
