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
    Buffer_ReadOnly = 0x2,
};

enum_flags(int, LineFlags)
{
    Line_Empty = 0x1,
};

struct LineData
{
    Range range;
    int64_t newline_pos;
    LineFlags flags;
    int token_count;
    Token *tokens;
};

#define TEXTIT_BUFFER_SIZE Megabytes(128)
#define BUFFER_ASYNC_THRESHOLD Megabytes(4)
struct Buffer : TextStorage
{
    BufferID id;
    BufferFlags flags;

    bool bulk_edit;
    uint64_t undo_batch_ordinal;

    Arena arena;
    String name;

    LineEndKind line_end;

    struct
    {
        int64_t current_ordinal;
        uint64_t depth;
        UndoNode root;
        UndoNode *at;

        int64_t run_pos;
        int64_t insert_pos;
        FixedStringContainer<2048> fwd_buffer;
        FixedStringContainer<2048> bck_buffer;
    } undo;

    struct LanguageSpec *language;
    struct IndentRules  *indent_rules;

    LineData null_line_data;

    VirtualArray<Token, 32'000'000> tokens;
    VirtualArray<LineData, 8'000'000> line_data;
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
    return ((line >= 0) && (line < buffer->line_data.count));
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
MakeTokenIterator(Buffer *buffer, int index, int count = 0)
{
    int max_count = buffer->tokens.count - index;
    if (count <= 0 || count > max_count) count = max_count;

    TokenIterator result = {};
    result.tokens = &buffer->tokens[index];
    result.tokens_end = &result.tokens[count];
    result.token = result.tokens;
    return result;
}

function TokenIterator
SeekTokenIterator(Buffer *buffer, int64_t start_pos = 0)
{
    TokenIterator result = {};
    result.tokens     = buffer->tokens.data;
    result.tokens_end = result.tokens + buffer->tokens.count;

    int64_t token_count = buffer->tokens.count;
    for (int index = 0; index < token_count; index += 1)
    {
        Token *t = &buffer->tokens[index];
        if (t->pos >= start_pos)
        {
            result.token = t;
            break;
        }
    }

    return result;
}

#endif /* TEXTIT_BUFFER_HPP */
