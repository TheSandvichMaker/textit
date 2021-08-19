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

    Rect2i viewport;
    int64_t actual_viewport_line_height;
    int64_t scroll_at;
    Range visible_range;

    uint64_t *line_hashes;
    uint64_t *prev_line_hashes;

    int jump_top;
    int jump_at;
    Jump jump_buffer[256];
};

function int
OldestJumpIndex(View *view)
{
    int index = view->jump_top - ArrayCount(view->jump_buffer);
    if (index < 0) index = 0;
    return index;
}

function Jump *
GetJump(View *view, int index)
{
    if (index >= view->jump_top)        index = view->jump_top - 1;
    if (index <  OldestJumpIndex(view)) index = OldestJumpIndex(view);
    return &view->jump_buffer[index % ArrayCount(view->jump_buffer)];
}

function View *GetView(ViewID id);

#endif /* TEXTIT_VIEW_HPP */
