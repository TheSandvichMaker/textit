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

#if 0
struct Cursors
{
    size_t count;
    Cursor **cursors;

    Cursor *&operator[](size_t i) const
    {
        Assert(i < count);
        return cursors[i];
    }

    Cursor **begin() const { return cursors; }
    Cursor **end() const { return cursors + count; }
};
#else
#endif

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
    CursorHashEntry *prev_in_hash;

    CursorHashKey key;
    Cursor *cursor;
};

#define CURSOR_HASH_SIZE 512

struct CursorTable
{
    Cursor *first_free_cursor;
    CursorHashEntry *first_free_cursor_hash_entry;
    CursorHashEntry *cursor_hash[CURSOR_HASH_SIZE];
};

GLOBAL_STATE(CursorTable, cursor_table);

function Cursor *CreateCursor(View *view);
function void DestroyCursors(ViewID view, BufferID buffer);
function void FreeCursor(Cursor *cursor);
function Cursor **GetCursorSlot(ViewID view, BufferID buffer, bool make_if_missing);
function Cursor *GetCursor(ViewID view, BufferID buffer);
function Cursor *GetCursor(View *view, Buffer *buffer = nullptr);
function Cursors GetCursors(Arena *arena, View *view, Buffer *buffer = nullptr);
function Cursor *IterateCursors(ViewID view, BufferID buffer);
function Cursor *IterateCursors(View *view);
function void OrganizeCursorsAfterEdit(View *view, Buffer *buffer);

#endif /* TEXTIT_CURSOR_HPP */
