#ifndef TEXTIT_TYPES_HPP
#define TEXTIT_TYPES_HPP

#include <stdint.h>
#include <stddef.h>
#include <float.h>

#include "textit_math_types.hpp"

struct Arena
{
    size_t capacity;
    size_t committed;
    size_t used;
    char *base;
    uint32_t temp_count;
};

struct String
{
    size_t size;
    uint8_t *data;
};

constexpr String
operator ""_str(const char *data, size_t size)
{
    String result = {};
    result.size = size;
    result.data = (uint8_t *)data;
    return result;
}

#define StringLiteral(c_string) Paste(c_string, _str)
#define StringExpand(string) (int)(string).size, (char *)(string).data

static inline String
MakeString(size_t size, uint8_t *text)
{
    String result;
    result.size = size;
    result.data = text;
    return result;
}

struct String_utf16
{
    size_t size;
    wchar_t *data;
};

constexpr String_utf16
operator ""_str16(const wchar_t *data, size_t size)
{
    String_utf16 result = {};
    result.size = size;
    result.data = (wchar_t *)data;
    return result;
}

struct StringNode
{
    StringNode *next;
    String string;
};

struct StringList
{
    StringNode *first = nullptr;
    StringNode *last = nullptr;
    size_t total_size = 0;
    size_t node_count = 0;
};

struct StringContainer
{
    union
    {
        struct
        {
            size_t size;
            uint8_t *data;
        };
        String string;
    };
    size_t capacity;
};

struct Range
{
    // convention is [start .. end)
    int64_t start, end;
};

static inline Range
MakeRange(int64_t start, int64_t end)
{
    Range result;
    result.start = start;
    result.end = end;
    return result;
}

static inline Range
MakeRange(int64_t pos)
{
    return MakeRange(pos, pos);
}

static inline Range
MakeSanitaryRange(int64_t start, int64_t end)
{
    Range result;
    if (start <= end)
    {
        result.start = start;
        result.end = end;
    }
    else
    {
        result.start = end;
        result.end = start;
    }
    return result;
}

static inline Range
SanitizeRange(Range range)
{
    if (range.end < range.start)
    {
        Swap(range.end, range.start);
    }
    return range;
}

static inline Range
MakeRangeStartLength(int64_t start, int64_t length)
{
    return MakeRange(start, start + length);
}

static inline int64_t
ClampToRange(int64_t value, Range bounds)
{
    if (value < bounds.start) value = bounds.start;
    if (value > bounds.end  ) value = bounds.end;
    return value;
}

static inline Range
ClampRange(Range range, Range bounds)
{
    if (range.start < bounds.start) range.start = bounds.start;
    if (range.start > bounds.end  ) range.start = bounds.end;
    if (range.end   < bounds.start) range.end   = bounds.start;
    if (range.end   > bounds.end  ) range.end   = bounds.end;
    return range;
}

static inline int64_t
RangeSize(Range range)
{
    range = SanitizeRange(range);
    return range.end - range.start;
}

// NOTE: These colors are in BGRA byte order
struct Color
{
    union
    {
        uint32_t u32;

        struct
        {
            uint8_t b, g, r, a;
        };
    };
};

struct Bitmap
{
    int32_t w, h, pitch;
    Color *data;
};

struct Font
{
    union
    {
        struct
        {
            int32_t w, h, pitch;
            Color *data;
        };
        Bitmap bitmap;
    };

    int32_t glyph_w, glyph_h;
    uint32_t glyphs_per_row, glyphs_per_col, glyph_count;
};

#endif /* TEXTIT_TYPES_HPP */
