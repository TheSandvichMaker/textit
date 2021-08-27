#ifndef TEXTIT_LINE_INDEX_HPP
#define TEXTIT_LINE_INDEX_HPP

#define VALIDATE_LINE_INDEX_TREE_INTEGRITY_AGGRESSIVELY 1

enum_flags(uint8_t, LineFlags)
{
    Line_Empty = 0x1,
};

enum_flags(uint8_t, LineTokenizeState)
{
    LineTokenizeState_None         = 0x0,
    LineTokenizeState_BlockComment = 0x1,
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

struct LineIndexIterator
{
    LineIndexNode *record;
    Range          range;
    int64_t        line; 
};

struct TokenIterator
{
    TokenBlock *block;
    int64_t     index;
    Token       token;
};

function int64_t GetLineCount(struct Buffer *buffer);
function bool ValidateLineIndexFull(Buffer *buffer);
function bool ValidateLineIndexTreeIntegrity(LineIndexNode *root);

#endif /* TEXTIT_LINE_INDEX_HPP */
