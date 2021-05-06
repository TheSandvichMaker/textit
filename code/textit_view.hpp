#ifndef TEXTIT_VIEW_HPP
#define TEXTIT_VIEW_HPP

struct View
{
    Arena arena;
    Buffer *buffer;
    V2i cursor;
};

#endif /* TEXTIT_VIEW_HPP */
