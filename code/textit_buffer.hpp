#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

#define MAX_BUFFER_COUNT 1024

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

struct BufferLocation
{
    int64_t pos;
    int64_t line_number;
    Range line_range;
};

typedef uint32_t BufferFlags;
enum BufferFlags_ENUM : BufferFlags
{
    Buffer_Indestructible = 0x1,
    Buffer_ReadOnly = 0x2,
};

#define TEXTIT_BUFFER_SIZE Megabytes(128)
#define BUFFER_ASYNC_THRESHOLD Megabytes(2)
struct Buffer
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
    } undo;

    struct LanguageSpec *language;

    BufferCursor cursor;
    BufferCursor mark;
    
    bool dirty;
    volatile bool tokenizing;

    volatile TokenList *tokens;
    TokenList *prev_tokens;
    TokenList tokens_[2];

    int64_t count;
    int64_t committed;
    int64_t capacity;
    uint8_t *text;
};

static inline Buffer *GetBuffer(BufferID id);

static inline TokenIterator
MakeTokenIterator(Buffer *buffer, int64_t start_pos = 0)
{
    return MakeTokenIterator((TokenList *)buffer->tokens, start_pos);
}

#endif /* TEXTIT_BUFFER_HPP */
