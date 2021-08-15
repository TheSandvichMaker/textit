function Tag *
AddTag(Tags *tags, Buffer *buffer, String name)
{
    Tag *result = PushStruct(&editor->transient_arena, Tag);

    HashResult hash = HashString(name);
    result->hash = hash;

    uint32_t slot = hash.u32[0] % ArrayCount(editor->tag_table);
    result->next_in_hash = editor->tag_table[slot];
    editor->tag_table[slot] = result;

    DllInsertBack(&tags->sentinel, result);

    result->buffer = buffer->id;

    return result;
}

function void
ParseCppTags(Buffer *buffer)
{
    Tags *tags = buffer->tags;
    if (!tags->sentinel.next)
    {
        DllInit(&tags->sentinel);
    }

    TokenIterator it = IterateTokens(buffer);
    while (IsValid(&it))
    {
        Token *t = Next(&it);
        if (!t) break;

        ScopedMemory temp(platform->GetTempArena());
        String text = PushBufferRange(temp, buffer, MakeRangeStartLength(t->pos, t->length));

        if (AreEqual(text, "struct"_str))
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
}

function Tag *
FindTag(String name)
{
    HashResult hash = HashString(name);

    Tag *result = editor->tag_table[hash.u32[0] % ArrayCount(editor->tag_table)];
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
