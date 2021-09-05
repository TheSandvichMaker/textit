#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

#define MAX_BUFFER_COUNT 1024

struct Project;
struct LanguageSpec;
struct IndentRules;
struct Tags;
struct Tag;

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

typedef int BufferFlags;
enum BufferFlags_ENUM : BufferFlags
{
    Buffer_Indestructible = 0x1,
    Buffer_ReadOnly       = 0x2,
    Buffer_Hidden         = 0x4,
};

#define TEXTIT_BUFFER_SIZE Gigabytes(8)
#define BUFFER_ASYNC_THRESHOLD Megabytes(4)
struct Buffer : TextStorage
{
    BufferID id;
    BufferFlags flags;

    bool dirty;

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

    int64_t last_save_undo_ordinal;

    Project      *project;
    LanguageSpec *inferred_language;
    LanguageSpec *language;
    IndentRules  *indent_rules;
    Tags         *tags;

    TokenBlock *first_free_token_block;
    Tag *first_free_tag;

    LineIndexNode *line_index_root;
    LineIndexNode *first_free_line_index_node;
};

function TokenBlock *
AllocateTokenBlock(Buffer *buffer)
{
    if (!buffer->first_free_token_block)
    {
        buffer->first_free_token_block = PushStructNoClear(&buffer->arena, TokenBlock);
        buffer->first_free_token_block->next = nullptr;
    }
    TokenBlock *result = SllStackPop(buffer->first_free_token_block);
    ZeroStruct(result);
    return result;
}

function void
FreeTokenBlock(Buffer *buffer, TokenBlock *block)
{
    block->token_count = TOKEN_BLOCK_FREE_TAG;
    SllStackPush(buffer->first_free_token_block, block);
}

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
    return ((line >= 0) && (line < GetLineCount(buffer)));
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

function uint8_t ReadBufferByte(Buffer *buffer, int64_t pos);
function Buffer *DEBUG_FindWhichBufferThisMemoryBelongsTo(void *memory_init);

#endif /* TEXTIT_BUFFER_HPP */
