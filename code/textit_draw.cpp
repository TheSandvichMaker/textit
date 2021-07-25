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

    bool draw_cursor = is_active_window;

    bool visualize_newlines              = core_config->visualize_newlines;
    bool right_align_visualized_newlines = core_config->right_align_visualized_newlines; (void)right_align_visualized_newlines;
    bool visualize_whitespace            = core_config->visualize_whitespace; (void)visualize_whitespace;
    bool show_line_numbers               = core_config->show_line_numbers;

    Color text_comment                 = GetThemeColor("text_comment"_str);
    Color text_preprocessor            = GetThemeColor("text_preprocessor"_str);
    Color text_foreground              = GetThemeColor("text_foreground"_str);
    Color text_foreground_dim          = GetThemeColor("text_foreground_dim"_str);
    Color text_foreground_dimmer       = GetThemeColor("text_foreground_dimmer"_str);
    Color text_foreground_dimmest      = GetThemeColor("text_foreground_dimmest"_str);
    Color text_background              = GetThemeColor("text_background"_str);
    Color text_background_unreachable  = GetThemeColor("text_background_unreachable"_str);
    Color text_background_highlighted  = GetThemeColor("text_background_highlighted"_str);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_str);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_str);
    Color selection_background         = GetThemeColor("selection_background"_str);

    Rect2i inner_bounds = bounds;
    inner_bounds.min += MakeV2i(2, 1);
    inner_bounds.max -= MakeV2i(2, 1);

    V2i top_left = inner_bounds.min;

    int64_t buffer_line_count = buffer->line_data.count;

    int64_t min_line = view->scroll_at;
    int64_t max_line = view->scroll_at + GetHeight(inner_bounds);

    min_line = Clamp(min_line, 0, buffer_line_count);
    max_line = Clamp(max_line, 0, buffer_line_count);

    int64_t max_line_digits = Log10(max_line) + 1;
    int64_t line_width      = GetWidth(inner_bounds);

    int64_t actual_line_height = 0;
    int64_t row = 0;

    if (view->scroll_at < 0)
    {
        row += -view->scroll_at;
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x + 1, bounds.min.y + 1),
                                  MakeV2i(bounds.max.x - 1, top_left.y + row)),
                 text_background_unreachable);
        actual_line_height += -view->scroll_at;
    }

    for (int64_t line = min_line; line < max_line; line += 1)
    {
        LineData *data = &buffer->line_data[line];

        if ((line + 1 == max_line) &&
            RangeSize(data->range) == 0)
        {
            break;
        }

        actual_line_height += 1;

        Token *token     = data->tokens;
        Token *token_end = data->tokens + data->token_count;

        int64_t start = data->range.start;
        int64_t end   = data->range.end;
        int64_t col   = 0;

        int64_t line_number = line + 1;

        if (show_line_numbers)
        {
            DrawLine(top_left + MakeV2i(col, row), PushTempStringF("%-4lld", line_number), text_foreground_dimmest, text_background);
            col += max_line_digits + 1;
        }

        for (int64_t pos = start; pos < end;)
        {
            uint8_t b = ReadBufferByte(buffer, pos);

            Color foreground = text_foreground;
            Color background = text_background;

            //
            // Colorize Tokens
            //

            while (token &&
                   (pos >= (token->pos + token->length)))
            {
                token++;
                if (token >= token_end)
                {
                    token = nullptr;
                    break;
                }
            }

            if (token)
            {
                if (HasFlag(token->flags, TokenFlag_IsComment))
                {
                    foreground = text_comment;
                }
                else if (token->kind == Token_Identifier)
                {
                    if (HasFlag(token->flags, TokenFlag_IsPreprocessor))
                    {
                        foreground = text_preprocessor;
                    }
                    else
                    {
                        String string = MakeString(token->length, &buffer->text[token->pos]);
                        TokenKind kind = (TokenKind)PointerToInt(StringMapFind(buffer->language->idents, string));
                        if (kind)
                        {
                            foreground = GetThemeColor(TokenThemeName(kind));
                            token->kind = kind; // TODO: questionable! deferring the lookup to only do visible areas is good,
                                                // but maybe it should be moved outside of the actual drawing function
                        }
                    }
                }
                else
                {
                    foreground = GetThemeColor(TokenThemeName(token->kind));
                }

                if (pos >= token->pos &&
                    editor_state->pos_at_mouse >= token->pos &&
                    editor_state->pos_at_mouse <  token->pos + token->length)
                {
                    editor_state->token_at_mouse = token;
                    background = text_background_highlighted;
                }
            }

            String string = {};

            int64_t advance     = 1;
            bool    right_align = false;

            if (b == '\t')
            {
                col += 4;
            }
            else if ((b == '\r' || b == '\n') &&visualize_newlines)
            {
                if (b == '\r' && ReadBufferByte(buffer, pos + 1) == '\n')
                {
                    string = PushTempStringF("\\r\\n");
                    advance = 2;
                }
                else if (b == '\r')
                {
                    string = PushTempStringF("\\r");
                }
                else if (b == '\n')
                {
                    string = PushTempStringF("\\n");
                }
                else
                {
                    INVALID_CODE_PATH;
                }
                foreground = text_foreground_dimmest;
                background = text_background;
                right_align = right_align_visualized_newlines;
            }
            else if (IsUtf8Byte(b))
            {
                ParseUtf8Result unicode = ParseUtf8Codepoint(&buffer->text[pos]);
                string = PushTempStringF("\\u%x", unicode.codepoint);
                advance = unicode.advance;
                foreground = unrenderable_text_foreground;
                background = unrenderable_text_background;
            }
            else
            {
                string = MakeString(1, &buffer->text[pos]);
            }

            if (draw_cursor)
            {
                for (Cursor *cursor = IterateCursors(view->id, buffer->id);
                     cursor;
                     cursor = cursor->next)
                {
                    Range selection = SanitizeRange(cursor->selection);

                    if (pos >= selection.start &&
                        pos <= selection.end)
                    {
                        background = selection_background;
                        foreground.r = 255 - selection_background.r;
                        foreground.g = 255 - selection_background.g;
                        foreground.b = 255 - selection_background.b;
                    }

                    if (pos == cursor->pos)
                    {
                        Swap(foreground, background);
                        break;
                    }
                }
            }

            if (right_align)
            {
                col = line_width - string.size;
            }

            for (size_t i = 0; i < string.size; ++i)
            {
                PushTile(Layer_Text, top_left + MakeV2i(col, row), MakeSprite(string.data[i], foreground, background));

                if (AreEqual(editor_state->text_mouse_p, top_left + MakeV2i(col, row)))
                {
                    editor_state->pos_at_mouse = pos;
                }

                col += 1;
                if (pos + advance < end &&
                    col >= line_width)
                {
                    PushTile(Layer_Text, top_left + MakeV2i(col, row), MakeSprite('\\', MakeColor(127, 127, 127), text_background));

                    col = -1;
                    row += 1;

                    if (top_left.y + row >= inner_bounds.max.y)
                    {
                        return actual_line_height;
                    }
                }
            }

            if (top_left.y + row >= inner_bounds.max.y)
            {
                return actual_line_height;
            }

            pos += advance;
        }

        row += 1;
    }

    if (top_left.y + row < inner_bounds.max.y)
    {
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x + 1, top_left.y + row),
                                  MakeV2i(bounds.max.x - 1, bounds.max.y)),
                 text_background_unreachable);
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

    PushRect(Layer_Text, MakeRect2iMinMax(bounds.min + MakeV2i(1, 1), bounds.max - MakeV2i(1, 1)), text_background);

    int64_t actual_line_height = DrawTextArea(view, bounds, is_active_window);

    PushRectOutline(Layer_Text, bounds, text_foreground, text_background);
    DrawLine(MakeV2i(bounds.min.x + 2, bounds.min.y),
             PushTempStringF("%hd:%.*s - pos: %lld, scroll: %d, line: %lld, col: %lld, repeat: %lld, mouse: { %lld, %lld }, %lld",
                             buffer->id.index, StringExpand(buffer->name),
                             cursor->pos,
                             view->scroll_at,
                             loc.line, loc.col,
                             view->repeat_value,
                             editor_state->text_mouse_p.x,
                             editor_state->text_mouse_p.y,
                             editor_state->pos_at_mouse),
             filebar_text_foreground, filebar_text_background, GetWidth(bounds) - 4);

    int64_t scan_line = Clamp(view->scroll_at, 0, buffer->line_data.count);

    int64_t line_count = buffer->line_data.count;
    int64_t bounds_height = GetHeight(bounds) - 2;
    int64_t scrollbar_size = Max(1, bounds_height*bounds_height / line_count);
    int64_t scrollbar_offset = Min(bounds_height - scrollbar_size, scan_line*bounds_height / line_count);

    for (int i = 0; i < scrollbar_size; i += 1)
    {
        PushTile(Layer_Text,
                 MakeV2i(bounds.max.x - 1, bounds.min.y + i + 1 + scrollbar_offset),
                 MakeWall(Wall_Top|Wall_Bottom, text_foreground, MakeColor(127, 127, 127)));
    }

    return actual_line_height;
}

function void
DrawCommandLineInput()
{
    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);

    V2i p = MakeV2i(2, render_state->viewport.max.y - 1);
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

            DrawLine(p + MakeV2i(0, -prediction_offset), other_prediction->name, color, text_background);
            prediction_offset += 1;
        }
    }

    for (int i = 0; i < editor_state->command_line_count + 1; i += 1)
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
