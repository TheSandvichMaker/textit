#ifndef TEXTIT_CURSOR_HPP
#define TEXTIT_CURSOR_HPP

struct Cursor
{
    V2i visual_pos;

    Cursor *next;
    int64_t sticky_col;

    int64_t pos;
    Selection selection;
};

function void SetCursor(Cursor *cursor, int64_t pos, Range inner = {}, Range outer = {});
function void SetSelection(Cursor *cursor, Range inner, Range outer = {});

struct CursorHashKey
{
    union
    {
        struct
        {
            ViewID view;
            BufferID buffer;
        };

        uint64_t value;
    };
};

struct CursorHashEntry
{
    union
    {
        CursorHashEntry *next_in_hash;
        CursorHashEntry *next_free;
    };

    CursorHashKey key;
    Cursor *cursor;
};

function Cursor *CreateCursor(View *view);
function void DestroyCursors(ViewID view, BufferID buffer);
function void FreeCursor(Cursor *cursor);
function Cursor **GetCursorSlot(ViewID view, BufferID buffer, bool make_if_missing);
function Cursor *GetCursor(ViewID view, BufferID buffer);
function Cursor *GetCursor(View *view, Buffer *buffer = nullptr);
function Cursor *IterateCursors(ViewID view, BufferID buffer);
function Cursor *IterateCursors(View *view);

#endif /* TEXTIT_CURSOR_HPP */
