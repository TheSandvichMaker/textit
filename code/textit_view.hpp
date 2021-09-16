#ifndef TEXTIT_VIEW_HPP
#define TEXTIT_VIEW_HPP

#define MAX_VIEW_COUNT 128

struct Jump
{
    BufferID buffer;
    int64_t  pos;
};

struct View
{
    ViewID id;

    Arena arena;
    BufferID buffer;
    BufferID next_buffer;

    int64_t sticky_col;
    int64_t repeat_value;

    bool center_view_next_time_we_calculate_scroll; // terrible
    bool adjust_cursor_to_view; // terrible

    Rect2i viewport;
    int64_t actual_viewport_line_height;
    int64_t scroll_at;
    Range visible_range;

    uint64_t *line_hashes;
    uint64_t *prev_line_hashes;

    uint32_t jump_top;
    uint32_t jump_at;
    Jump jump_buffer[256];
};

function uint32_t
OldestJumpIndex(View *view)
{
    uint32_t result = 0;
    if (view->jump_top >= ArrayCount(view->jump_buffer))
    {
        result = view->jump_top - ArrayCount(view->jump_buffer);
    }
    return result;
}

function Jump *
GetJump(View *view, uint32_t index)
{
    if (view->jump_top > 0 && index >= view->jump_top) index = view->jump_top - 1;
    if (index < OldestJumpIndex(view))                 index = OldestJumpIndex(view);
    return &view->jump_buffer[index % ArrayCount(view->jump_buffer)];
}

function View *GetView(ViewID id);
function View *OpenNewView(BufferID buffer);
function View *GetActiveView(void);

struct ViewIterator
{
    size_t index;
    View *view;
};

function ViewIterator IterateViews(void);
function bool IsValid(ViewIterator *iter);
function void Next(ViewIterator *iter);

#endif /* TEXTIT_VIEW_HPP */
