#ifndef TEXTIT_VIEW_HPP
#define TEXTIT_VIEW_HPP

#define MAX_VIEW_COUNT 128

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
};

static inline View *GetView(ViewID id);

#endif /* TEXTIT_VIEW_HPP */
