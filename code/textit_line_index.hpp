#ifndef TEXTIT_LINE_INDEX_HPP
#define TEXTIT_LINE_INDEX_HPP

enum_flags(uint8_t, LineFlags)
{
    Line_Empty = 0x1,
};

enum_flags(uint8_t, LineTokenizeState)
{
};

struct LineData
{
    Range             range;          // 16
    int64_t           newline_pos;    // 24
    LineFlags         flags;          // 25
    LineTokenizeState tokenize_state; // 26
    int16_t           token_count;    // 28
    uint32_t          token_index;    // 32
};

enum LineIndexNodeKind : uint8_t
{
    LineIndexNode_Internal,
    LineIndexNode_Leaf,
};

struct LineIndexKey
{
    int64_t pos  : 48;
    int64_t line : 24;
};

#define LINE_INDEX_ARITY 8

struct LineIndexNode
{
    LineIndexNodeKind kind; // 1
    int8_t child_count;    // 2

    LineIndexNode *parent;
    LineIndexKey keys[LINE_INDEX_ARITY];

    union
    {
        LineIndexNode *children[LINE_INDEX_ARITY]; // 128
        LineData *records[LINE_INDEX_ARITY]; // 128
    };
};

#endif /* TEXTIT_LINE_INDEX_HPP */
