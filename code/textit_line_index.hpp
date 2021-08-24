#ifndef TEXTIT_LINE_INDEX_HPP
#define TEXTIT_LINE_INDEX_HPP

enum_flags(uint8_t, LineFlags)
{
    Line_Empty = 0x1,
};

enum_flags(uint8_t, LineTokenizeState)
{
};

struct LineInfo
{
    int64_t   line;
    Range     range;
    LineFlags flags;
    int16_t   token_count;
    uint32_t  token_index;
};

struct LineData
{
    union
    {
        LineData *next; // next free

        struct
        {
            Range             range;          // 16
            int64_t           newline_pos;    // 24
            LineFlags         flags;          // 25
            LineTokenizeState tokenize_state; // 26
            int16_t           token_count;    // 28
            uint32_t          token_index;    // 32
        };
    };
};

enum LineIndexNodeKind : uint8_t
{
    LineIndexNode_Leaf,
    LineIndexNode_Internal,
    LineIndexNode_Record,
};

#define LINE_INDEX_ORDER 4

struct LineIndexNode
{
    LineIndexNodeKind kind;
    int8_t entry_count;

    int64_t span;
    int64_t line_span;

    union
    {
        struct
        {
            LineIndexNode *paddding_for_next[2*LINE_INDEX_ORDER];
            LineIndexNode *next;
        };

        // NOTE: I am exploting the fact that in order to have the next pointer threaded through,
        // I have an extra slot anyway. This extra slot can double as "spillage" when a node overflows
        // so I can avoid having extra logic having to insert an overflowing entry into the correct
        // node post-split. -24/08/2021
        LineIndexNode *children[2*LINE_INDEX_ORDER + 1];

        LineData data;
    };
};

struct LineIndex
{
    Arena *arena; // I don't like this
    LineIndexNode *root;
};

struct LineIndexIterator
{
    LineIndexNode *leaf, *record;
    int            index;
    Range          range;
    int64_t        line; 
};

function int64_t
GetLineCount(LineIndex *index)
{
    return index->root ? index->root->line_span : 0;
}

#endif /* TEXTIT_LINE_INDEX_HPP */
