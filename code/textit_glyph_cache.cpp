function void
ResizeGlyphCache(GlyphCache *cache, uint32_t size)
{
    if (cache->size)
    {
        Release(&cache->arena);
        ZeroStruct(cache);
    }
    
    cache->size = size;
    cache->lookup_table = PushArray(&cache->arena, 4*size / 3, uint32_t);
    cache->entries      = PushArray(&cache->arena, size, GlyphEntry);

    for (size_t i = 0; i < size; i += 1)
    {
        GlyphEntry *entry = &cache->entries[i];
        entry->index = (uint32_t)i;
        entry->next_in_hash = ((uint32_t)i + 1) % size;
    }
}

function GlyphEntry *
GetGlyphEntry(GlyphCache *cache, HashResult hash)
{
    Assert(cache->size);

    GlyphEntry *sentinel = cache->entries;
    GlyphEntry *result   = nullptr;

    {
        uint32_t index = cache->lookup_table[hash.u32[0] % cache->size];
        while (index)
        {
            GlyphEntry *entry = &cache->entries[index];
            if (entry->hash == hash)
            {
                result = entry;
                break;
            }
            index = entry->next_in_hash;
        }
    }

    if (!result)
    {
        if (sentinel->next_in_hash)
        {
            result = &cache->entries[sentinel->next_in_hash];
            sentinel->next_in_hash = result->next_in_hash;
            result->next_in_hash = 0;
        }
        else
        {
            result = &cache->entries[sentinel->next_lru];

            // remove result from lru
            cache->entries[result->next_lru].prev_lru = result->prev_lru;
            cache->entries[result->prev_lru].next_lru = result->next_lru;

            // evict old entry if there is one
            uint32_t *prev_index_at = &cache->lookup_table[result->hash.u32[0] % cache->size];
            while (*prev_index_at)
            {
                GlyphEntry *entry = &cache->entries[*prev_index_at];
                if (entry->hash == result->hash)
                {
                    *prev_index_at = entry->next_in_hash;
                    entry->next_in_hash = 0;
                    break;
                }
                prev_index_at = &entry->next_in_hash;
            }
        }

        result->hash  = hash;
        result->state = GlyphState_Empty;

        uint32_t *index_at = &cache->lookup_table[hash.u32[0] % cache->size];
        result->next_in_hash = *index_at;
        *index_at = result->index;
    }

    // place result at end of lru because we just used it
    result->next_lru = 0;
    result->prev_lru = sentinel->prev_lru;
    cache->entries[result->next_lru].prev_lru = result->index;
    cache->entries[result->prev_lru].next_lru = result->index;

    return result;
}

function uint32_t
GetEntryCount(GlyphCache *cache, GlyphState state_to_count)
{
    uint32_t result = 0;
    for (size_t i = 0; i < cache->size; i += 1)
    {
        if (cache->entries[i].state == state_to_count)
        {
            result += 1;
        }
    }
    return result;
}
