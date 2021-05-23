static inline V2i
DrawLine(V2i p, String line, Color foreground, Color background)
{
    V2i at_p = p;
    for (size_t i = 0; i < line.size;)
    {
        if (IsUtf8Byte(line.data[i]))
        {
            ParseUtf8Result unicode = ParseUtf8Codepoint(&line.data[i]);
            String string = FormatTempString("\\u%x", unicode.codepoint);
            at_p = DrawLine(at_p, string, foreground, background);
            i += unicode.advance;
        }
        else if (IsPrintableAscii(line.data[i]))
        {
            Sprite sprite = MakeSprite(line.data[i], foreground, background);
            PushTile(Layer_Text, at_p, sprite);

            at_p.x += 1;
            i += 1;
        }
        else
        {
            String string = FormatTempString("\\x%02hhx", line.data[i]);
            at_p = DrawLine(at_p, string, foreground, background);
            i += 1;
        }
    }
    return at_p;
}

static inline void
DrawTextArea(View *view, Rect2i bounds)
{
    Buffer *buffer = GetBuffer(view);

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t left = bounds.min.x + 2;
    V2i at_p = MakeV2i(left, bounds.max.y - 2);

    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_str);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_str);

    int64_t scan_line = 0;
    int64_t pos = 0;
    while ((scan_line < view->scroll_at) &&
           (pos < buffer->count))
    {
        int64_t newline_length = PeekNewline(buffer, pos);
        if (newline_length)
        {
            pos += newline_length;
            scan_line += 1;
        }
        else
        {
            pos += 1;
        }
    }

    while (pos < buffer->count)
    {
        if (at_p.y <= bounds.min.y)
        {
            break;
        }

        if (pos == loc.pos)
        {
            PushTile(Layer_Text, at_p, MakeSprite('\0', text_background, text_foreground));
        }

        if (buffer->text[pos] == '\0')
        {
            break;
        }

        int64_t newline_length = PeekNewline(buffer, pos);
        if (newline_length)
        {
            at_p.x = left;
            at_p.y -= 1;

            pos += newline_length;
        }
        else
        {
            if (at_p.x >= (bounds.max.x - 2))
            {
                PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));
                at_p.x = left - 1;
                at_p.y -= 1;
            }

            uint8_t b = buffer->text[pos];
            if (IsAsciiByte(b))
            {
                Sprite sprite = MakeSprite(buffer->text[pos], text_foreground, text_background);
                if (pos == loc.pos)
                {
                    Swap(sprite.foreground, sprite.background);
                }
                PushTile(Layer_Text, at_p, sprite);
                at_p.x += 1;
                pos += 1;
            }
            else
            {
                Color foreground = unrenderable_text_foreground;
                Color background = unrenderable_text_background;
                if (pos == loc.pos)
                {
                    Swap(foreground, background);
                }
                ParseUtf8Result unicode = ParseUtf8Codepoint(&buffer->text[pos]);
                String string = FormatTempString("\\u%x", unicode.codepoint);
                for (size_t i = 0; i < string.size; ++i)
                {
                    PushTile(Layer_Text, at_p, MakeSprite(string.data[i], foreground, background));
                    at_p.x += 1;
                    if (at_p.x >= (bounds.max.x - 2))
                    {
                        PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));
                        at_p.x = left - 1;
                        at_p.y -= 1;
                    }
                }
                pos += unicode.advance;
            }
        }
    }
}

static inline void
DrawView(View *view)
{
    Buffer *buffer = GetBuffer(view);
    Rect2i bounds = view->viewport;

    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);
    Color filebar_text_foreground = GetThemeColor("filebar_text_foreground"_str);

    String filebar_text_background_str = "filebar_text_background"_str;
    switch (editor_state->edit_mode)
    {
        case EditMode_Text:
        {
            filebar_text_background_str = "filebar_text_background_text_mode"_str;
        } break;
    }
    Color filebar_text_background = GetThemeColor(filebar_text_background_str);

    PushRectOutline(Layer_Background, bounds, text_foreground, text_background);
    DrawLine(MakeV2i(bounds.min.x + 2, bounds.max.y - 1),
             FormatTempString("%hd:%.*s - scroll: %d", buffer->id.index, StringExpand(buffer->name), view->scroll_at),
             filebar_text_foreground, filebar_text_background);

    DrawTextArea(view, bounds);
#if 0
    PushRectOutline(Layer_Background, right, text_foreground, text_background);

    V2i at_p = MakeV2i(right.min.x + 2, right.max.y - 2);

    UndoNode *current = CurrentUndoNode(buffer);
    DrawLine(at_p, FormatTempString("At level: %llu", buffer->undo.depth), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Ordinal: %llu, Pos: %lld", current->ordinal, current->pos), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Forward: \"%.*s\"", StringExpand(current->forward)), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Backward: \"%.*s\"", StringExpand(current->backward)), text_foreground, text_background); at_p.y -= 1;
    at_p.y -= 1;

    uint32_t branch_index = 0;
    for (UndoNode *child = current->first_child;
         child;
         child = child->next_child)
    {
        DrawLine(at_p, FormatTempString("Branch %d%s", branch_index, (branch_index == current->selected_branch ? " (NEXT)" : "")), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Ordinal: %llu, Pos: %lld", child->ordinal, child->pos), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Forward: \"%.*s\"", StringExpand(child->forward)), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Backward: \"%.*s\"", StringExpand(child->backward)), text_foreground, text_background); at_p.y -= 1;
        branch_index += 1;
        at_p.y -= 1;
    }
#endif
}
