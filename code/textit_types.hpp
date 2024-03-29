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

    uint8_t
    operator[](size_t index)
    {
        if (index < size)
        {
            return data[index];
        }
        else
        {
            return 0;
        }
    }
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

function String
MakeString(size_t size, uint8_t *text)
{
    String result;
    result.size = size;
    result.data = text;
    return result;
}

function String
MakeString(uint8_t *start, uint8_t *one_past_end)
{
	Assert(start <= one_past_end);
	
	String result;
	result.data = start;
	result.size = one_past_end - start;
	return result;
}

struct String16
{
    size_t size;
    wchar_t *data;
};

constexpr String16
operator ""_str16(const wchar_t *data, size_t size)
{
    String16 result = {};
    result.size = size;
    result.data = (wchar_t *)data;
    return result;
}

function String16
MakeString16(size_t size, wchar_t *text)
{
    String16 result;
    result.size = size;
    result.data = text;
    return result;
}

function String16
MakeString16(wchar_t *text)
{
    String16 result;
    result.size = 0;
    result.data = text;
    for (wchar_t *c = text; *c; c++)
    {
        result.size++;
    }
    return result;
}

struct StringContainer
{
    size_t capacity;
    union
    {
        struct
        {
            size_t size;
            uint8_t *data;
        };
        String as_string;
    };
};

function StringContainer
MakeStringContainer(size_t capacity, uint8_t *storage)
{
    StringContainer result = {};
    result.capacity = capacity;
    result.data     = storage;
    return result;
}

struct StringNode
{
    StringNode *next;
    String string;
};

struct StringList
{
    Arena *arena;
    StringNode *first;
    StringNode *last;
    size_t total_size;
    size_t node_count;
};

struct Range
{
    // convention is [start .. end)
    int64_t start, end;
    
    operator bool() { return start != end; } // questionable decision, I use ranges with the same start and end from time to time
};

struct RangeNode
{
    RangeNode *next;
    Range range;
};

function Range
MakeRange(int64_t start, int64_t end)
{
    Range result;
    result.start = start;
    result.end = end;
    return result;
}

function Range
MakeRangeInclusive(int64_t start, int64_t end)
{
    Range result;
    result.start = start;
    result.end = end + 1;
    return result;
}

function Range
MakeRange(int64_t pos)
{
    return MakeRange(pos, pos);
}

function Range
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

function Range
SanitizeRange(Range range)
{
    if (range.end < range.start)
    {
        Swap(range.end, range.start);
    }
    return range;
}

function Range
MakeRangeStartLength(int64_t start, int64_t length)
{
    return MakeRange(start, start + length);
}

function Range
InvertedInfinityRange()
{
    Range result;
    result.start = INT64_MAX;
    result.end   = INT64_MIN;
    return result;
}

function int64_t
ClampToRange(int64_t value, Range bounds)
{
    if (value >= bounds.end  ) value = bounds.end - 1;
    if (value <  bounds.start) value = bounds.start;
    return value;
}

function Range
ClampRange(Range range, Range bounds)
{
    if (range.start < bounds.start) range.start = bounds.start;
    if (range.start > bounds.end  ) range.start = bounds.end;
    if (range.end   < bounds.start) range.end   = bounds.start;
    if (range.end   > bounds.end  ) range.end   = bounds.end;
    return range;
}

function int64_t
RangeSize(Range range)
{
    range = SanitizeRange(range);
    return range.end - range.start;
}

function Range
TrimEnd(Range range, int64_t trim)
{
    range.end -= trim;
    if (range.end < range.start) range.end = range.start;
    return range;
}

function Range
Union(Range a, Range b)
{
    Range result;
    result.start = a.start < b.start ? a.start : b.start;
    result.end = a.end > b.end ? a.end : b.end;
    return result;
}

function bool
IsInRange(Range range, int64_t pos)
{
    return pos >= range.start && pos < range.end;
}

function bool
RangesOverlap(Range a, Range b)
{
    return !(b.start >= a.end ||
             a.start >= b.end ||
             a.end <= b.start ||
             b.end <= a.start);
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
