#ifndef TEXTIT_VIEW_HPP
#define TEXTIT_VIEW_HPP

#define MAX_VIEW_COUNT 128

struct View
{
    ViewID id;

    Arena arena;
    BufferID buffer;

    V2i cursor;

    Rect2i viewport;
    int64_t scroll_at;
};

#endif /* TEXTIT_VIEW_HPP */
