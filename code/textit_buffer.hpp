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

    LineData null_line_data;

    VirtualArray<Token> tokens;
    VirtualArray<LineData> line_data;
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

function uint32_t
FindTokenIndexForPos(Buffer *buffer, int64_t pos)
{
    Assert(IsInBufferRange(buffer, pos));

    uint32_t result = 0;

    if (pos != 0)
    {
        int64_t token_count = buffer->tokens.count;
        int64_t lo = 0;
        int64_t hi = token_count - 1;
        while (lo <= hi)
        {
            int64_t index = (lo + hi) / 2;
            Token *t = &buffer->tokens[index];
            if (pos < t->pos)
            {
                hi = index - 1;
            }
            else if (pos >= t->pos + t->length)
            {
                lo = index + 1;
            }
            else
            {
                result = (uint32_t)index;
                break;
            }
        }

        if (lo > hi)
        {
            result = (uint32_t)lo;
        }
    }

    return result;
}

function Token *
GetTokenAt(Buffer *buffer, int64_t pos)
{
    uint32_t index = FindTokenIndexForPos(buffer, pos);
    Token *token = &buffer->tokens[index];
    return token;
}

function TokenIterator
IterateTokens(Buffer *buffer, int64_t pos = 0)
{
    pos = ClampToBufferRange(buffer, pos);

    TokenIterator result = {};
    result.tokens     = buffer->tokens.data;
    result.tokens_end = result.tokens + buffer->tokens.count;
    result.token      = &buffer->tokens[FindTokenIndexForPos(buffer, pos)];

    return result;
}

function TokenIterator
IterateLineTokens(Buffer *buffer, int64_t line)
{
    LineData *line_data = GetLineData(buffer, line);

    TokenIterator result = {};
    result.tokens     = &buffer->tokens[line_data->token_index];
    result.tokens_end = result.tokens + line_data->token_count;
    result.token      = result.tokens;

    return result;
}

#endif /* TEXTIT_BUFFER_HPP */
