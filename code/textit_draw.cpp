function V2i
DrawLine(V2i p, String line, Color foreground, Color background, size_t max = 0)
{
    V2i at_p = p;
    for (size_t i = 0; i < line.size;)
    {
        if (max > 0 && i >= max)
        {
            break;
        }

        if (IsUtf8Byte(line.data[i]))
        {
            ParseUtf8Result unicode = ParseUtf8Codepoint(&line.data[i]);
            String string = PushTempStringF("\\u%x", unicode.codepoint);
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
            String string = PushTempStringF("\\x%02hhx", line.data[i]);
            at_p = DrawLine(at_p, string, foreground, background);
            i += 1;
        }
    }
    return at_p;
}

function int64_t
DrawTextArea(View *view, Rect2i bounds, bool is_active_window)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t cursor_pos = cursor->pos;

    bool draw_cursor = is_active_window;

    int64_t left = bounds.min.x + 2;
    V2i at_p = MakeV2i(left, bounds.max.y - 2);

    Color text_comment                 = GetThemeColor("text_comment"_str);
    Color text_preprocessor            = GetThemeColor("text_preprocessor"_str);
    Color text_foreground              = GetThemeColor("text_foreground"_str);
    Color text_background              = GetThemeColor("text_background"_str);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_str);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_str);
    Color selection_background         = GetThemeColor("selection_background"_str);

    int64_t actual_line_height = 0;

    if (view->scroll_at < 0)
    {
        at_p.y += view->scroll_at;
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x + 1, at_p.y + 1),
                                  MakeV2i(bounds.max.x - 1, bounds.max.y - 1)),
                 MakeColor(32, 32, 32));
        actual_line_height += ((bounds.max.y - 1) - at_p.y);
    }

    Range mark_range = GetEditRange(cursor);

    TokenBlock *block = buffer->tokens->first;

    int64_t token_index = 0;

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
        if (block)
        {
            if (pos >= block->max_pos)
            {
                block = block->next;
            }
        }
    }

    int64_t line_count = buffer->line_count;
    int64_t bounds_height = GetHeight(bounds) - 2;
    int64_t scrollbar_size = Max(1, bounds_height*bounds_height / line_count);
    int64_t scrollbar_offset = Min(bounds_height - scrollbar_size, scan_line*bounds_height / line_count);

    for (int i = 0; i < scrollbar_size; i += 1)
    {
        PushTile(Layer_Text, MakeV2i(bounds.max.x - 1, bounds.max.y - 2 - i - scrollbar_offset), MakeWall(Wall_Top|Wall_Bottom, text_foreground, MakeColor(127, 127, 127)));
    }

    if (block)
    {
        while (token_index < block->count)
        {
            Token *t = &block->tokens[token_index];
            if (pos >= t->pos + t->length)
            {
                token_index += 1;
            }
            else
            {
                break;
            }
        }
    }

    Token null_token = {};
    null_token.kind = Token_Identifier;
    null_token.pos  = INT64_MAX;

    Token *token = (block ? &block->tokens[token_index] : &null_token);
    while (pos < buffer->count)
    {
        if (pos >= token->pos + token->length)
        {
            token_index += 1;

            if (block &&
                token_index >= block->count)
            {
                block = block->next;
                token_index = 0;
            }

            if (block)
            {
                token = &block->tokens[token_index];
            }
            else
            {
                token = &null_token;
            }
        }

        Color color = text_foreground;
        if (HasFlag(token->flags, TokenFlag_IsComment))
        {
            color = text_comment;
        }
        else if ((token->kind == Token_Identifier) &&
            HasFlag(token->flags, TokenFlag_IsPreprocessor))
        {
            color = text_preprocessor;
        }
        else
        {
            color = GetThemeColor(TokenThemeName(token->kind));
        }

        if (draw_cursor && (pos == cursor_pos))
        {
            PushTile(Layer_Text, at_p, MakeSprite('\0', text_background, text_foreground));
        }

        if (buffer->text[pos] == '\0')
        {
            break;
        }

        if (at_p.x >= (bounds.max.x - 2))
        {
            PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));

            at_p.x = left - 1;
            at_p.y -= 1;

            if (at_p.y <= bounds.min.y)
            {
                return actual_line_height;
            }
        }

        uint8_t b = buffer->text[pos];
        if (b == ' ' || IsPrintableAscii(b))
        {
            Sprite sprite = MakeSprite(buffer->text[pos], color, text_background);
            if (draw_cursor &&
                (editor_state->edit_mode == EditMode_Command) &&
                (pos >= mark_range.start && pos <= mark_range.end))
            {
                sprite.background = selection_background;
                sprite.foreground.r = 255 - selection_background.r;
                sprite.foreground.g = 255 - selection_background.g;
                sprite.foreground.b = 255 - selection_background.b;
            }
            if (draw_cursor && (pos == cursor_pos))
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
            if (draw_cursor && (pos == cursor_pos))
            {
                Swap(foreground, background);
            }

            if (draw_cursor &&
                (editor_state->edit_mode == EditMode_Command) &&
                (pos >= mark_range.start && pos <= mark_range.end))
            {
                background = selection_background;
                foreground.r = 255 - selection_background.r;
                foreground.g = 255 - selection_background.g;
                foreground.b = 255 - selection_background.b;
            }

            int64_t advance = 1;
            String string = {};
            if (b == '\r')
            {
                if (core_config->visualize_newlines) string = PushTempStringF("\\r");
            }
            else if (b == '\n')
            {
                if (core_config->visualize_newlines) string = PushTempStringF("\\n");
            }
            else
            {
                ParseUtf8Result unicode = ParseUtf8Codepoint(&buffer->text[pos]);
                string = PushTempStringF("\\u%x", unicode.codepoint);
                advance = unicode.advance;
            }
            for (size_t i = 0; i < string.size; ++i)
            {
                PushTile(Layer_Text, at_p, MakeSprite(string.data[i], foreground, background));
                at_p.x += 1;
                if (at_p.x >= (bounds.max.x - 2))
                {
                    PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));

                    at_p.x = left - 1;
                    at_p.y -= 1;

                    if (at_p.y <= bounds.min.y)
                    {
                        return actual_line_height;
                    }
                }
            }
            pos += advance;
        }

        if (b == '\n')
        {
            at_p.x = left;
            at_p.y -= 1;

            if (at_p.y > bounds.min.y)
            {
                actual_line_height += 1;
            }
            else
            {
                return actual_line_height;
            }
        }
    }

    if (at_p.y > bounds.min.y)
    {
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x + 1, bounds.min.y + 1),
                                  MakeV2i(bounds.max.x - 1, at_p.y + 1)),
                 MakeColor(32, 32, 32));
        actual_line_height += (at_p.y - bounds.min.y);
    }


    return actual_line_height;
}

function int64_t
DrawView(View *view, bool is_active_window)
{
    Buffer *buffer = GetBuffer(view);
    Rect2i bounds = view->viewport;

    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);
    Color filebar_text_foreground = GetThemeColor("filebar_text_foreground"_str);

    String filebar_text_background_str = "filebar_text_inactive"_str;
    if (is_active_window)
    {
        filebar_text_background_str = "filebar_text_background"_str;
        switch (editor_state->edit_mode)
        {
            case EditMode_Text:
            {
                filebar_text_background_str = "filebar_text_background_text_mode"_str;
            } break;

            default:
            {
                /* crickets */
            } break;
        }
    }

    Color filebar_text_background = GetThemeColor(filebar_text_background_str);

    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    PushRectOutline(Layer_Text, bounds, text_foreground, text_background);
    DrawLine(MakeV2i(bounds.min.x + 2, bounds.max.y - 1),
             PushTempStringF("%hd:%.*s - scroll: %d, line: %lld, col: %lld",
                             buffer->id.index, StringExpand(buffer->name),
                             view->scroll_at,
                             loc.line, loc.col),
             filebar_text_foreground, filebar_text_background, GetWidth(bounds) - 4);

    return DrawTextArea(view, bounds, is_active_window);
}

function void
DrawCommandLineInput()
{
    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);

    V2i p = MakeV2i(2, 0);
    if (editor_state->command_line_prediction_count > 0)
    {
        int prediction_offset = 1;
        for (int i = 0; i < editor_state->command_line_prediction_count; i += 1)
        {
            Command *other_prediction = editor_state->command_line_predictions[i];

            Color color = MakeColor(127, 127, 127);
            if (i == editor_state->command_line_prediction_selected_index)
            {
                color = MakeColor(192, 127, 127);
            }

            DrawLine(p + MakeV2i(0, prediction_offset), other_prediction->name, color, text_background);
            prediction_offset += 1;
        }
    }

    for (size_t i = 0; i < editor_state->command_line_count + 1; i += 1)
    {
        Sprite sprite;
        if (i < editor_state->command_line_count)
        {
            sprite = MakeSprite(editor_state->command_line[i], text_foreground, text_background);
        }
        else
        {
            sprite = MakeSprite(0, text_foreground, text_background);
        }
        if (i == editor_state->command_line_cursor)
        {
            Swap(sprite.foreground, sprite.background);
        }
        PushTile(Layer_Text, p + MakeV2i(i, 0), sprite);
    }
}
