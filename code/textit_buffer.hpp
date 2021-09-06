#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

#define MAX_BUFFER_COUNT 1024

struct Project;
struct LanguageSpec;
struct IndentRules;
struct Tags;
struct Tag;
struct Cursor;

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

function Buffer *OpenNewBuffer(String buffer_name, BufferFlags flags = 0);
function Buffer *OpenBufferFromFile(String filename, BufferFlags flags = 0);
function Buffer *OpenBufferFromFileAsync(PlatformJobQueue *queue, String filename, BufferFlags flags = 0);
function Buffer *GetBuffer(BufferID id);
function Buffer *GetActiveBuffer(void);

function Range BufferRange(Buffer *buffer);
function bool IsInBufferRange(Buffer *buffer, int64_t pos);
function int64_t ClampToBufferRange(Buffer *buffer, int64_t pos);
function bool LineIsInBuffer(Buffer *buffer, int64_t line);
function uint8_t ReadBufferByte(Buffer *buffer, int64_t pos);
function int64_t PeekNewline(Buffer *buffer, int64_t pos);
function int64_t PeekNewlineBackward(Buffer *buffer, int64_t pos);
function bool AdvanceOverNewline(Buffer *buffer, int64_t *pos);
function Selection ScanWordForward(Buffer *buffer, int64_t pos);
function int64_t ScanWordEndForward(Buffer *buffer, int64_t pos);
function Selection ScanWordBackward(Buffer *buffer, int64_t pos);
function int64_t FindLineStart(Buffer *buffer, int64_t pos);
function void FindLineEnd(Buffer *buffer, int64_t pos, int64_t *inner, int64_t *outer = nullptr);
function int64_t FindFirstNonHorzWhitespace(Buffer *buffer, int64_t pos);
function Range EncloseLine(Buffer *buffer, int64_t pos, bool including_newline = false);
function Range GetLineRange(Buffer *buffer, int64_t line);
function Range GetInnerLineRange(Buffer *buffer, int64_t line);
function int64_t GetLineNumber(Buffer *buffer, int64_t pos);
function Range GetLineRange(Buffer *buffer, Range range);
function void GetLineRanges(Buffer *buffer, int64_t line, Range *inner, Range *outer);
function BufferLocation CalculateBufferLocationFromPos(Buffer *buffer, int64_t pos);
function BufferLocation CalculateBufferLocationFromLineCol(Buffer *buffer, int64_t line, int64_t col);
function BufferLocation CalculateRelativeMove(Buffer *buffer, Cursor *cursor, V2i delta);
function void MergeUndoHistory(Buffer *buffer, int64_t first_ordinal, int64_t last_ordinal);
function void BeginUndoBatch(Buffer *buffer);
function void EndUndoBatch(Buffer *buffer);

function String PushBufferRange(Arena *arena, Buffer *buffer, Range range);
function String PushBufferRange(StringContainer *container, Buffer *buffer, Range range);
function String PushTokenString(Arena *arena, Buffer *buffer, Token *t);
function String PushTokenString(StringContainer *container, Buffer *buffer, Token *t);

function int64_t BufferReplaceRangeNoUndoHistory(Buffer *buffer, Range range, String text);
function int64_t BufferReplaceRange(Buffer *buffer, Range range, String text);

function Range FindNextOccurrence(Buffer *buffer, int64_t pos, String query, StringMatchFlags flags = 0);
function Range FindPreviousOccurrence(Buffer *buffer, int64_t pos, String query, StringMatchFlags flags = 0);

function UndoNode *CurrentUndoNode(Buffer *buffer);
function int64_t CurrentUndoOrdinal(Buffer *buffer);
function void PushUndo(Buffer *buffer, int64_t pos, String forward, String backward);
function void FlushBufferedUndo(Buffer *buffer);
function void SelectNextUndoBranch(Buffer *buffer);
function UndoNode *NextChild(UndoNode *node);

struct BufferIterator
{
    size_t index;
    Buffer *buffer;
};

function BufferIterator IterateBuffers(void);
function bool IsValid(BufferIterator *iter);
function void Next(BufferIterator *iter);

function Buffer *DEBUG_FindWhichBufferThisMemoryBelongsTo(void *memory_init);

#endif /* TEXTIT_BUFFER_HPP */
