function Window *
AllocateWindow(void)
{
    if (!editor->first_free_window)
    {
        editor->first_free_window = PushStructNoClear(&editor->transient_arena, Window);
        editor->first_free_window->next = nullptr;

        editor->debug.allocated_window_count += 1;
    }
    Window *result = editor->first_free_window;
    editor->first_free_window = result->next;

    ZeroStruct(result);
    return result;
}

function void
SplitWindow(Window *window, WindowSplitKind split)
{
    Assert(window->split == WindowSplit_Leaf);

    ViewID view = window->view;

    if (window->parent &&
        window->parent->split == split)
    {
        Window *new_window = AllocateWindow();
        new_window->view = DuplicateView(view);
        new_window->parent = window->parent;
        new_window->prev = window;
        new_window->next = window->next;
        if (new_window->next)
        {
            new_window->next->prev = new_window;
        }
        else
        {
            new_window->parent->last_child = new_window;
        }
        window->next = new_window;

        editor->active_window = window;
    }
    else
    {
        window->split = split;

        Window *left = AllocateWindow();
        left->parent = window;
        left->view = view;

        Window *right = AllocateWindow();
        right->parent = window;
        right->view = DuplicateView(view);

        left->next = right;
        right->prev = left;
        window->first_child = left;
        window->last_child  = right;

        editor->active_window = left;
    }
}

function void
DestroyWindowInternal(Window *window)
{
    if (window->parent)
    {
        Window *parent = window->parent;
        if (window->prev) window->prev->next = window->next;
        if (window->next) window->next->prev = window->prev;
        if (window == parent->first_child) parent->first_child = window->next;
        if (window == parent->last_child)  parent->last_child  = window->prev;

        if (window->prev)
        {
            editor->active_window = window->prev;
        }
        else if (window->next)
        {
            editor->active_window = window->next;
        }
        else
        {
            editor->active_window = parent->first_child;
        }

        if (!parent->first_child)
        {
            if (window->split == WindowSplit_Leaf)
            {
                DestroyView(window->view);
            }

            DestroyWindowInternal(parent);
        }
        else if (parent->first_child == parent->last_child)
        {
            // NOTE: If the parent is a split with only one child,
            // we want to condense that so that the parent just
            // gets replaced by its child.
            Window *first_child = parent->first_child;

            first_child->parent = parent->parent;
            parent->split       = first_child->split;
            parent->view        = first_child->view;
            parent->first_child = parent->first_child->first_child;
            parent->last_child  = parent->last_child->last_child;

            first_child->next = editor->first_free_window;
            editor->first_free_window = first_child;

            for (Window *child = parent->first_child;
                 child;
                 child = child->next)
            {
                child->parent = parent;
            }

            editor->active_window = parent;
        }
        else if (window->split == WindowSplit_Leaf)
        {
            DestroyView(window->view);
        }

        window->next = editor->first_free_window;
        editor->first_free_window = window;
    }
}

function void
DestroyWindow(Window *window)
{
    DestroyWindowInternal(window);
    if (editor->active_window->split != WindowSplit_Leaf)
    {
        editor->active_window = editor->active_window->first_child;
    }
}

function void
RecalculateViewBounds(Window *window, Rect2i rect)
{
    if (window->split == WindowSplit_Leaf)
    {
        View *view = GetView(window->view);
        int64_t line_count = view->viewport.max.y - view->viewport.min.y;
        if (AreEqual(view->viewport.min, rect.min) &&
            AreEqual(view->viewport.max, rect.max))
        {
            Swap(view->prev_line_hashes, view->line_hashes);
            ZeroArray(line_count, view->line_hashes);
        }
        else
        {
            ZeroArray(line_count, view->line_hashes);
            for (int64_t i = 0; i < line_count; i += 1)
            {
                view->prev_line_hashes[i] = 0xBEEF;
            }
        }
        view->viewport = rect;
    }
    else
    {
        int child_count = 0;
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            child_count += 1;
        }

        int64_t dim   = ((window->split == WindowSplit_Vert) ? GetWidth(rect) : GetHeight(rect));
        int64_t start = ((window->split == WindowSplit_Vert) ? rect.min.x : rect.min.y);
        int64_t end   = ((window->split == WindowSplit_Vert) ? rect.max.x : rect.max.y);

        int child_index = 0;
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            int64_t split_start = start + child_index*dim / child_count;
            int64_t split_end   = start + (child_index + 1)*dim / child_count;
            if (!child->next)
            {
                split_end = end;
            }

            Rect2i child_rect = rect;
            if (window->split == WindowSplit_Vert)
            {
                child_rect.min.x = split_start;
                child_rect.max.x = split_end;
            }
            else
            {
                child_rect.min.y = split_start;
                child_rect.max.y = split_end;
            }
            RecalculateViewBounds(child, child_rect);

            child_index += 1;
        }
    }
}

function void
DrawWindows(Window *window)
{
    if (window->split == WindowSplit_Leaf)
    {
        View *view = GetView(window->view);
        Buffer *buffer = GetBuffer(view);

        int64_t estimated_viewport_line_height = view->actual_viewport_line_height;
        if (!estimated_viewport_line_height)
        {
            estimated_viewport_line_height = Max(0, view->viewport.max.y - view->viewport.min.y - 1);
        }

        {
            int64_t top = Max(0, view->scroll_at + core_config->view_autoscroll_margin);
            int64_t bot = Max(0, view->scroll_at + estimated_viewport_line_height - 2 - core_config->view_autoscroll_margin);

            int64_t cursors_min_line = INT64_MAX;
            int64_t cursors_max_line = INT64_MIN;
            for (Cursor *cursor = IterateCursors(view);
                 cursor;
                 cursor = cursor->next)
            {
                BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
                if (cursors_min_line > loc.line)
                {
                    cursors_min_line = loc.line;
                }
                if (cursors_max_line < loc.line)
                {
                    cursors_max_line = loc.line;
                }
            }

            int64_t delta = 0;

            if (cursors_min_line < top)
            {
                delta = cursors_min_line - top;
            }
            if (cursors_max_line > bot)
            {
                delta = cursors_max_line - bot;
            }

            if (view->adjust_cursor_to_view)
            {
                view->adjust_cursor_to_view = false;
                for (Cursor *cursor = IterateCursors(view);
                     cursor;
                     cursor = cursor->next)
                {
                    int64_t pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, -delta)).pos;
                    cursor->pos = pos;
                }
            }
            else if (delta != 0)
            {
                view->scroll_at += delta;
                if (delta < 0)
                {
                    if (view->scroll_at < 0) view->scroll_at = 0;
                }
                else
                {
                    int64_t maximum_viewport_line_height = Max(0, view->viewport.max.y - view->viewport.min.y - 1);
                    int64_t line_count = GetLineCount(buffer);
                    int64_t maximum_line = line_count - maximum_viewport_line_height;
                    if (view->scroll_at > maximum_line) view->scroll_at = maximum_line;
                }
            }

            if (view->center_view_next_time_we_calculate_scroll)
            {
                int64_t cursor_count = 0;
                int64_t line_mean = 0;
                for (Cursor *cursor = IterateCursors(view);
                     cursor;
                     cursor = cursor->next)
                {
                    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
                    cursor_count += 1;
                    line_mean    += loc.line;
                }
                line_mean /= cursor_count;

                view->center_view_next_time_we_calculate_scroll = false;
                view->scroll_at = line_mean - estimated_viewport_line_height / 2;

                int64_t maximum_viewport_line_height = Max(0, view->viewport.max.y - view->viewport.min.y - 1);
                int64_t line_count = GetLineCount(buffer);
                int64_t maximum_line = line_count - maximum_viewport_line_height;
                if (view->scroll_at < 0) view->scroll_at = 0;
                if (view->scroll_at > maximum_line) view->scroll_at = maximum_line;
            }
        }

        bool is_active_window = (window == editor->active_window);

        ScopedClipRect clip_rect(Scale(view->viewport, editor->font_metrics), view->id);
        view->actual_viewport_line_height = DrawView(view, is_active_window);
    }
    else
    {
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            DrawWindows(child);
        }
    }
}
