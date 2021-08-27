#ifndef TEXTIT_LINE_INDEX_HPP
#define TEXTIT_LINE_INDEX_HPP

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
        LineIndexNode *children[2*LINE_INDEX_ORDER + 2];
        LineData data;
    };
};

struct LineIndexIterator
{
    LineIndexNode *leaf, *record;
    int            index;
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
function bool ValidateLineIndex(Buffer *buffer);
function bool ValidateLineIndex(LineIndexNode *root, Buffer *buffer);

#endif /* TEXTIT_LINE_INDEX_HPP */
