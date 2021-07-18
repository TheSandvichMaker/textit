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

struct BufferLocation
{
    int64_t pos;
    int64_t col;
    int64_t line;
    Range line_range;
};

typedef uint32_t BufferFlags;
enum BufferFlags_ENUM : BufferFlags
{
    Buffer_Indestructible = 0x1,
    Buffer_ReadOnly = 0x2,
};

typedef uint32_t LineFlags;
enum LineFlags_ENUM : LineFlags
{
    Line_Empty = 0x1,
};

struct LineData
{
    Range range;
    int64_t whitespace_pos;
    LineFlags flags;
    int token_offset;
    int token_count;
    TokenBlock *tokens;
};

#define TEXTIT_BUFFER_SIZE Megabytes(128)
#define BUFFER_ASYNC_THRESHOLD Megabytes(4)
struct Buffer : TextStorage
{
    BufferID id;
    BufferFlags flags;

    Arena arena;
    String name;

    LineEndKind line_end;

    struct
    {
        uint64_t current_ordinal;
        uint64_t depth;
        UndoNode root;
        UndoNode *at;

        int64_t run_pos;
        int64_t insert_pos;
        FixedStringContainer<2048> fwd_buffer;
        FixedStringContainer<2048> bck_buffer;
    } undo;

    struct LanguageSpec *language;

    int line_count;
    TokenList tokens;

    LineData null_line_data;
    LineData line_data[1000000];
};

function Buffer *GetBuffer(BufferID id);
function Range GetLineRange(Buffer *buffer, int64_t line);
function LineData *GetLineData(Buffer *buffer, int64_t line);

function bool
IsInBufferRange(Buffer *buffer, int64_t pos)
{
    bool result = ((pos >= 0) && (pos < buffer->count));
    return result;
}

function bool
LineIsInBuffer(Buffer *buffer, int64_t line)
{
    return ((line >= 0) && (line < buffer->line_count));
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

function TokenIterator
MakeTokenIterator(Buffer *buffer, int64_t start_pos = 0)
{
    return MakeTokenIterator(&buffer->tokens, start_pos);
}

function TokenIterator
MakeTokenIterator(LineData *line_data)
{
    TokenIterator result = MakeTokenIterator(line_data->tokens, line_data->token_offset);
    return result;
}

function TokenIterator
IterateLineTokens(Buffer *buffer, int64_t line)
{
    LineData *line_data = GetLineData(buffer, line);
    TokenIterator result = MakeTokenIterator(line_data->tokens, line_data->token_offset, line_data->token_count);
    return result;
}

#endif /* TEXTIT_BUFFER_HPP */
