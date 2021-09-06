#ifndef TEXTIT_WINDOW_HPP
#define TEXTIT_WINDOW_HPP

enum WindowSplitKind
{
    WindowSplit_Leaf,
    WindowSplit_Horz,
    WindowSplit_Vert,
};

struct Window
{
    WindowSplitKind split;

    Window *parent;
    Window *first_child;
    Window *last_child;
    Window *next, *prev;

    ViewID view;
};

function void SplitWindow(Window *window, WindowSplitKind split);
function void DestroyWindow(Window *window);
function void RecalculateViewBounds(Window *window, Rect2i rect);
function void DrawWindows(Window *window);

#endif /* TEXTIT_WINDOW_HPP */
