#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

enum LineEndKind
{
    LineEnd_LF,
    LineEnd_CRLF,
};

static inline String
LineEndString(LineEndKind kind)
{
    switch (kind)
    {
        case LineEnd_LF:   return StringLiteral("\n");
        case LineEnd_CRLF: return StringLiteral("\r\n");
    }
    return StringLiteral("");
}

struct UndoNode
{
    UndoNode *parent;

    uint32_t selected_branch;
    UndoNode *first_child;
    UndoNode *next_child;

    uint64_t ordinal;

    int64_t pos;
    String forward;
    String backward;
};

struct UndoState
{
    uint64_t current_ordinal;
    uint64_t depth;
    UndoNode root;
    UndoNode *at;
};

struct BufferCursor
{
    int64_t pos;
};

#define TEXTIT_BUFFER_SIZE Megabytes(1)
struct Buffer
{
    Arena arena;
    String name;

    LineEndKind line_end;

    struct
    {
        uint64_t current_ordinal;
        uint64_t depth;
        UndoNode root;
        UndoNode *at;
    } undo;

    int64_t count;
    uint8_t text[TEXTIT_BUFFER_SIZE];
};

struct BufferLocation
{
    int64_t line_number;
    Range line_range;
    int64_t pos;
};

#endif /* TEXTIT_BUFFER_HPP */
