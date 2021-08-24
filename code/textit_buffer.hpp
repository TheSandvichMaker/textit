#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

#define MAX_BUFFER_COUNT 1024

enum LineEndKind
{
    LineEnd_LF,
    LineEnd_CRLF,
};

function String
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

    int64_t ordinal;

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

struct BufferLocation
{
    int64_t pos;
    int64_t col;
    int64_t line;
    Range line_range;
};

enum_flags(int, BufferFlags)
{
    Buffer_Indestructible = 0x1,
    Buffer_ReadOnly       = 0x2,
};

#define TEXTIT_BUFFER_SIZE Gigabytes(8)
#define BUFFER_ASYNC_THRESHOLD Megabytes(4)
struct Buffer : TextStorage
{
    BufferID id;
    BufferFlags flags;

    bool bulk_edit;
    uint64_t undo_batch_ordinal;

    Arena arena;
    String name;
    String full_path;

    LineEndKind line_end;

    struct
    {
        int64_t current_ordinal;
        uint64_t depth;
        UndoNode root;
        UndoNode *at;

        int64_t run_pos;
        int64_t insert_pos;
    } undo;

    struct Project      *project;
    struct LanguageSpec *language;
    struct IndentRules  *indent_rules;
    struct Tags         *tags;

    LineIndex line_index;
    VirtualArray<Token> tokens;
};

function Buffer *GetBuffer(BufferID id);
function Range GetLineRange(Buffer *buffer, int64_t line);

function bool
IsInBufferRange(Buffer *buffer, int64_t pos)
{
    bool result = ((pos >= 0) && (pos < buffer->count));
    return result;
}

function bool
LineIsInBuffer(Buffer *buffer, int64_t line)
{
    return ((line >= 0) && (line < GetLineCount(&buffer->line_index)));
}

function int64_t
ClampToBufferRange(Buffer *buffer, int64_t pos)
{
    if (pos >= buffer->count) pos = buffer->count - 1;
    if (pos < 0) pos = 0;
    return pos;
}

function Range
BufferRange(Buffer *buffer)
{
    return MakeRange(0, buffer->count);
}

function int64_t
GetLineCount(Buffer *buffer)
{
    return GetLineCount(&buffer->line_index);
}

#endif /* TEXTIT_BUFFER_HPP */
