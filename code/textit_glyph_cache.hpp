#ifndef TEXTIT_GLYPH_CACHE_HPP
#define TEXTIT_GLYPH_CACHE_HPP

enum GlyphState
{
    GlyphState_Empty,
    GlyphState_Sized,
    GlyphState_Filled,
};

struct GlyphEntry
{
    HashResult hash;

    uint32_t index;

    GlyphState state;
    int16_t glyph_size_x;
    int16_t glyph_size_y;

    uint32_t next_in_hash;
    uint32_t next_lru;
    uint32_t prev_lru;
};

struct GlyphCache
{
    Arena arena;

    uint32_t size;
    uint32_t *lookup_table; 
    GlyphEntry *entries;
};

#endif /* TEXTIT_GLYPH_CACHE_HPP */
