#ifndef TEXTIT_LINE_INDEX_HPP
#define TEXTIT_LINE_INDEX_HPP

#if TEXTIT_SLOW
#define VALIDATE_LINE_INDEX 1
#define VALIDATE_LINE_INDEX_TREE_INTEGRITY_AGGRESSIVELY 1
#define VALIDATE_LINE_INDEX_EXTRA_SLOW 1
#endif

struct Buffer;

enum_flags(uint8_t, LineFlags)
{
    Line_Empty = 0x1,
};

enum_flags(uint8_t, LineTokenizeState)
{
    // TODO: Use these states in the tokenizer to stop things from being extremely horrid
    LineTokenizeState_None         = 0x0,
    LineTokenizeState_BlockComment = 0x1,
    LineTokenizeState_Preprocessor = 0x2,
    LineTokenizeState_String       = 0x4,
};

struct LineData
{
    int64_t           newline_col;          
    LineFlags         flags;                
    LineTokenizeState start_tokenize_state; 
    LineTokenizeState end_tokenize_state;   
    TokenBlock       *first_token_block;
    TokenBlock       *last_token_block;
};

struct LineInfo
{
    int64_t   line;
    Range     range;
    int64_t   newline_pos;
    LineFlags flags;
    LineData *data;
};

enum LineIndexNodeKind : uint8_t
{
    LineIndexNode_Leaf,
    LineIndexNode_Internal,
    LineIndexNode_Record,
    LineIndexNode_FREE,
};

#define LINE_INDEX_ORDER 4

struct LineIndexNode
{
    LineIndexNodeKind kind;
    int8_t entry_count;

    LineIndexNode *parent;
    LineIndexNode *next, *prev;

    int64_t span;
    int64_t line_span;

    union
    {
        // NOTE: The + 1 is pure slop. It lets me overflow the node and _then_ split it without having
        // extra logic to insert the new entry into the right node post split
        LineIndexNode *children[2*LINE_INDEX_ORDER + 1];
        LineData data;
    };
};

function LineIndexNode *InsertLine(Buffer *buffer, Range range, LineData *data);
function void RemoveLinesFromIndex(Buffer *buffer, Range line_range);
function void MergeLines(Buffer *buffer, Range range);
function void ClearLineIndex(Buffer *buffer);

function void FindLineInfoByPos(Buffer *buffer, int64_t pos, LineInfo *out_info);
function void FindLineInfoByLine(Buffer *buffer, int64_t line, LineInfo *out_info);

function int64_t GetLineCount(Buffer *buffer);

function bool ValidateLineIndexFull(Buffer *buffer);
function bool ValidateLineIndexTreeIntegrity(LineIndexNode *root);
function bool ValidateTokenBlockChain(LineIndexNode *record);

struct LineIndexCountResult
{
    size_t nodes;
    size_t nodes_size;
    size_t token_blocks;
    size_t token_blocks_size;
    size_t token_blocks_capacity;
    size_t token_blocks_occupancy;
};

function void CountLineIndex(LineIndexNode *node, LineIndexCountResult *result);

struct LineIndexIterator
{
    LineIndexNode *record;
    Range          range;
    int64_t        line; 
};

function LineIndexIterator IterateLineIndex(Buffer *buffer);
function LineIndexIterator IterateLineIndexFromPos(Buffer *buffer, int64_t pos);
function LineIndexIterator IterateLineIndexFromLine(Buffer *buffer, int64_t line);
function bool IsValid(LineIndexIterator *it);
function void Next(LineIndexIterator *it);
function void Prev(LineIndexIterator *it);
function LineIndexNode *RemoveCurrent(LineIndexIterator *it);
function void GetLineInfo(LineIndexIterator *it, LineInfo *out_info);

#endif /* TEXTIT_LINE_INDEX_HPP */
