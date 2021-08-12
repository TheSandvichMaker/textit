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

    Rect2i viewport;
    int64_t actual_viewport_line_height;
    int64_t scroll_at;
    Range visible_range;

    uint64_t *line_hashes;
    uint64_t *prev_line_hashes;

    int jump_stack_at;
    int jump_stack_watermark;
    Jump jump_stack[128];
};

function View *GetView(ViewID id);

#endif /* TEXTIT_VIEW_HPP */
