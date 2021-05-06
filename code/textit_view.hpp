#ifndef TEXTIT_VIEW_HPP
#define TEXTIT_VIEW_HPP

struct View
{
    Arena arena;
    Buffer *buffer;
    V2i cursor;
    Rect2i viewport;
    int64_t scroll_at;
};

#endif /* TEXTIT_VIEW_HPP */
