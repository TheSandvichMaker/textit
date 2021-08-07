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

    bool draw_cursor    = is_active_window;
    bool draw_selection = (editor->edit_mode != EditMode_Text);

    bool visualize_newlines              = core_config->visualize_newlines;
    bool right_align_visualized_newlines = core_config->right_align_visualized_newlines; (void)right_align_visualized_newlines;
    bool visualize_whitespace            = core_config->visualize_whitespace;
    bool show_line_numbers               = core_config->show_line_numbers;
    int  indent_width                    = core_config->indent_width;

    Color text_comment                 = GetThemeColor("text_comment"_id);
    Color text_preprocessor            = GetThemeColor("text_preprocessor"_id);
    Color text_foreground              = GetThemeColor("text_foreground"_id);
    Color text_foreground_dim          = GetThemeColor("text_foreground_dim"_id);
    Color text_foreground_dimmer       = GetThemeColor("text_foreground_dimmer"_id);
    Color text_foreground_dimmest      = GetThemeColor("text_foreground_dimmest"_id);
    Color text_background              = GetThemeColor(is_active_window ? "text_background"_id : "text_background_inactive"_id);
    Color text_background_unreachable  = GetThemeColor("text_background_unreachable"_id);
    Color text_background_highlighted  = GetThemeColor("text_background_highlighted"_id);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_id);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_id);
    Color inner_selection_background   = GetThemeColor("inner_selection_background"_id);
    Color outer_selection_background   = GetThemeColor("outer_selection_background"_id);
    Color line_highlight               = GetThemeColor("line_highlight"_id);

    V2i top_left = bounds.min;

    int64_t buffer_line_count = buffer->line_data.count;

    int64_t min_line = view->scroll_at;
    int64_t max_line = view->scroll_at + GetHeight(bounds);

    min_line = Clamp(min_line, 0, buffer_line_count);
    max_line = Clamp(max_line, 0, buffer_line_count);

    int64_t max_line_digits = Log10(max_line) + 1;
    int64_t line_width      = GetWidth(bounds);

    int64_t actual_line_height = 0;
    int64_t row = 0;

    if (view->scroll_at < 0)
    {
        row += -view->scroll_at;
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x, bounds.min.y),
                                  MakeV2i(bounds.max.x, top_left.y + row)),
                 text_background_unreachable);
        actual_line_height += -view->scroll_at;
    }

    int64_t line = min_line;

    LineData null_data = {};
    LineData *data = &null_data;
    if (!buffer->line_data.Empty())
    {
        data = &buffer->line_data[min_line];
    }

    int64_t pos = data->range.start;
    view->visible_range.start = pos;

    while (IsInBufferRange(buffer, pos))
    {
        if ((line + 1 == max_line) &&
            RangeSize(data->range) == 0)
        {
            break;
        }

        if (pos >= data->range.end)
        {
            line += 1;
            if (line < max_line)
            {
                data = &buffer->line_data[line];
            }
            else
            {
                break;
            }
        }

        actual_line_height += 1;

        bool empty_line = data->newline_pos == data->range.start;

        Color base_background = text_background;

        if (draw_cursor)
        {
            for (Cursor *cursor = IterateCursors(view->id, buffer->id);
                 cursor;
                 cursor = cursor->next)
            {
                if (cursor->pos >= data->range.start && cursor->pos < data->range.end)
                {
                    base_background = line_highlight;
                    PushRect(Layer_Text,
                             MakeRect2iMinMax(MakeV2i(bounds.min.x, top_left.y + row),
                                              MakeV2i(bounds.max.x, top_left.y + row + 1)),
                             base_background);
                }
            }
        }

        Token *token     = &buffer->tokens[data->token_index];
        // Token *token_end = token + data->token_count;

        int64_t end   = data->range.end;
        int64_t col   = 0;

        int64_t line_number = line + 1;

        if (show_line_numbers)
        {
            DrawLine(top_left + MakeV2i(col, row), PushTempStringF("%-4lld", line_number), text_foreground_dimmest, base_background);
            col += Max(5, max_line_digits + 1);
        }

        int64_t left_col = col;

        while (IsInBufferRange(buffer, pos))
        {
            uint8_t b = ReadBufferByte(buffer, pos);

            Color foreground = text_foreground;
            Color background = base_background;

            //
            // Colorize Tokens
            //

            // TODO: Re-enable once confident things work
#if 0
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
#else
            // Getting the token like this is more robust against a busted token array, which is good
            // for when I'm iterating and want to see what my tokens are like
            token = &buffer->tokens[FindTokenIndexForPos(buffer, pos)];
#endif

            if (token)
            {
                foreground = GetThemeColor(TokenThemeIndex(token->kind));
                if (HasFlag(token->flags, TokenFlag_IsComment))
                {
                    foreground = text_comment;
                }
                else if (token->kind == Token_Identifier ||
                         token->kind == Token_Function)
                {
                    if (HasFlag(token->flags, TokenFlag_IsPreprocessor))
                    {
                        foreground = text_preprocessor;
                    }
                    else if (buffer->language)
                    {
                        String string = MakeString(token->length, &buffer->text[token->pos]);
                        StringID id = HashStringID(string);
                        TokenKind kind = GetTokenKindFromStringID(buffer->language, id);
                        if (kind)
                        {
                            foreground = GetThemeColor(TokenThemeIndex(kind));
                            token->kind = kind; // TODO: questionable! deferring the lookup to only do visible areas is good,
                                                // but maybe it should be moved outside of the actual drawing function
                        }
                    }
                }

                if (pos >= token->pos &&
                    editor->pos_at_mouse >= token->pos &&
                    editor->pos_at_mouse <  token->pos + token->length)
                {
                    editor->token_at_mouse = token;
                    background = text_background_highlighted;
                }
            }

            String string = {};
            if (empty_line)
            {
                string = " "_str;
            }

            int64_t advance          = 1;
            bool right_align         = false;
            bool encountered_newline = false;
            int64_t real_col         = col - left_col;

            int64_t newline_length = PeekNewline(buffer, pos);
            if (newline_length)
            {
                advance = newline_length;
                encountered_newline = true;
            }

            if (b == '\t')
            {
                if (visualize_whitespace)
                {
                    string = ">"_str;
                    foreground = text_foreground_dimmer;
                }
            }
            else if (b == '\r' || b == '\n')
            {
                if (visualize_newlines)
                {
                    if (b == '\r' && ReadBufferByte(buffer, pos + 1) == '\n')
                    {
                        string = "\\r\\n"_str;
                        advance = 2;
                    }
                    else if (b == '\r')
                    {
                        string = "\\r"_str;
                    }
                    else if (b == '\n')
                    {
                        string = "\\n"_str;
                    }
                    else
                    {
                        INVALID_CODE_PATH;
                    }
                    foreground = text_foreground_dimmest;
                    right_align = right_align_visualized_newlines;
                }
            }
            else if (b == ' ' && visualize_whitespace)
            {
                if (ReadBufferByte(buffer, pos - 1) == ' ' ||
                    ReadBufferByte(buffer, pos + 1) == ' ')
                {
                    string = PushTempStringF(".");
                    foreground = text_foreground_dimmer;
                }
                else
                {
                    string = MakeString(1, &buffer->text[pos]);
                }
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
                    if (pos == cursor->pos)
                    {
                        if (string.size == 0)
                        {
                            background = text_foreground;
                            string = " "_str;
                        }
                        else
                        {
                            Swap(foreground, background);
                        }
                        break;
                    }
                    else if (draw_selection)
                    {
                        Range inner = SanitizeRange(cursor->selection.inner);
                        Range outer = SanitizeRange(cursor->selection.outer);

                        if (pos >= inner.start &&
                            pos <  inner.end)
                        {
                            background = inner_selection_background;
                            foreground.r = 255 - inner_selection_background.r;
                            foreground.g = 255 - inner_selection_background.g;
                            foreground.b = 255 - inner_selection_background.b;
                        }
                        else if (pos >= outer.start &&
                                 pos <  outer.end)
                        {
                            background = outer_selection_background;
                            foreground.r = 255 - outer_selection_background.r;
                            foreground.g = 255 - outer_selection_background.g;
                            foreground.b = 255 - outer_selection_background.b;
                        }
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

                if (AreEqual(editor->text_mouse_p, top_left + MakeV2i(col, row)))
                {
                    editor->pos_at_mouse = pos;
                }

                col += 1;
                if (pos + advance < end &&
                    col >= line_width - 1)
                {
                    PushTile(Layer_Text, top_left + MakeV2i(col, row), MakeSprite('\\', MakeColor(127, 127, 127), text_background));

                    col  = 0;
                    row += 1;

                    if (top_left.y + row >= bounds.max.y)
                    {
                        return actual_line_height;
                    }
                }
            }

            if (b == '\t')
            {
                // TODO: tab logic needs to be done properly everywhere!!
                int64_t prev_col = col;
                col = left_col + RoundUpNextTabStop(real_col, indent_width);
                // TODO: This looks a bit janktastic when the cursor is on a tab
                for (int64_t i = prev_col; i < col; i += 1)
                {
                    PushTile(Layer_Text, top_left + MakeV2i(i, row), MakeSprite(' ', foreground, background));
                }
            }

            if (top_left.y + row >= bounds.max.y)
            {
                return actual_line_height;
            }

            pos += advance;

            if (encountered_newline)
            {
                break;
            }
        }

        row += 1;
    }
    view->visible_range.end = pos;

    if (top_left.y + row < bounds.max.y)
    {
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x, top_left.y + Max(0, row)),
                                  MakeV2i(bounds.max.x, bounds.max.y)),
                 text_background_unreachable);
    }

    return actual_line_height;
}

function int64_t
DrawTextArea2(View *view, Rect2i bounds, bool is_active_window)
{
    Buffer *buffer = GetBuffer(view);

    bool draw_cursor    = is_active_window;
    bool draw_selection = (editor->edit_mode != EditMode_Text);

    bool visualize_newlines              = core_config->visualize_newlines;
    bool right_align_visualized_newlines = core_config->right_align_visualized_newlines; (void)right_align_visualized_newlines;
    bool visualize_whitespace            = core_config->visualize_whitespace;
    bool show_line_numbers               = core_config->show_line_numbers;
    int  indent_width                    = core_config->indent_width;

    Color text_comment                 = GetThemeColor("text_comment"_id);
    Color text_preprocessor            = GetThemeColor("text_preprocessor"_id);
    Color text_foreground              = GetThemeColor("text_foreground"_id);
    Color text_foreground_dim          = GetThemeColor("text_foreground_dim"_id);
    Color text_foreground_dimmer       = GetThemeColor("text_foreground_dimmer"_id);
    Color text_foreground_dimmest      = GetThemeColor("text_foreground_dimmest"_id);
    Color text_background              = GetThemeColor(is_active_window ? "text_background"_id : "text_background_inactive"_id);
    Color text_background_unreachable  = GetThemeColor("text_background_unreachable"_id);
    Color text_background_highlighted  = GetThemeColor("text_background_highlighted"_id);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_id);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_id);
    Color inner_selection_background   = GetThemeColor("inner_selection_background"_id);
    Color outer_selection_background   = GetThemeColor("outer_selection_background"_id);
    Color line_highlight               = GetThemeColor("line_highlight"_id);

    V2i top_left = bounds.min;

    int64_t buffer_line_count = buffer->line_data.count;

    int64_t min_line = view->scroll_at;
    int64_t max_line = view->scroll_at + GetHeight(bounds);

    min_line = Clamp(min_line, 0, buffer_line_count);
    max_line = Clamp(max_line, 0, buffer_line_count);

    int64_t max_line_digits = Log10(max_line) + 1;
    int64_t line_width      = GetWidth(bounds);

    int64_t actual_line_height = 0;
    int64_t row = 0;

    if (view->scroll_at < 0)
    {
        row += -view->scroll_at;
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x, bounds.min.y),
                                  MakeV2i(bounds.max.x, top_left.y + row)),
                 text_background_unreachable);
        actual_line_height += -view->scroll_at;
    }

    int64_t line = min_line;

    LineData null_data = {};
    LineData *data = &null_data;
    if (!buffer->line_data.Empty())
    {
        data = &buffer->line_data[min_line];
    }

    int64_t pos = data->range.start;
    view->visible_range.start = pos;

    V2i metrics = editor->font_metrics;

    int64_t prev_col = 0;
    Color prev_background = {};
    Color prev_foreground = {};
    FixedStringContainer<1024> draw_buffer;

    ClearBitmap(&platform->backbuffer.bitmap);

    while (IsInBufferRange(buffer, pos))
    {
        if ((line + 1 == max_line) &&
            RangeSize(data->range) == 0)
        {
            break;
        }

        if (pos >= data->range.end)
        {
            line += 1;
            if (line < max_line)
            {
                data = &buffer->line_data[line];
            }
            else
            {
                break;
            }
        }

        actual_line_height += 1;

        bool empty_line = data->newline_pos == data->range.start;
        int64_t indentation_end = FindFirstNonHorzWhitespace(buffer, data->range.start);

        Color base_background = text_background;

        if (draw_cursor)
        {
            for (Cursor *cursor = IterateCursors(view->id, buffer->id);
                 cursor;
                 cursor = cursor->next)
            {
                if (cursor->pos >= data->range.start && cursor->pos < data->range.end)
                {
                    base_background = line_highlight;
                    PushRect(Layer_Text,
                             MakeRect2iMinMax(MakeV2i(bounds.min.x, top_left.y + row),
                                              MakeV2i(bounds.max.x, top_left.y + row + 1)),
                             base_background);
                }
            }
        }

        Token *token     = &buffer->tokens[data->token_index];
        // Token *token_end = token + data->token_count;

        // int64_t end   = data->range.end;
        int64_t col   = 0;

        int64_t line_number = line + 1;

        if (show_line_numbers)
        {
            PushText(Layer_Text, top_left + MakeV2i(col, row), PushTempStringF("%-4lld", line_number), text_foreground_dimmest, base_background);
            col += Max(5, max_line_digits + 1);
            prev_col = col;
        }

        int64_t left_col = col;

        while (IsInBufferRange(buffer, pos))
        {
            uint8_t b = ReadBufferByte(buffer, pos);

            Color foreground = text_foreground;
            Color background = base_background;

            //
            // Colorize Tokens
            //

            // TODO: Re-enable once confident things work
#if 0
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
#else
            // Getting the token like this is more robust against a busted token array, which is good
            // for when I'm iterating and want to see what my tokens are like
            token = &buffer->tokens[FindTokenIndexForPos(buffer, pos)];
#endif

            if (token)
            {
                foreground = GetThemeColor(TokenThemeIndex(token->kind));
                if (HasFlag(token->flags, TokenFlag_IsComment))
                {
                    foreground = text_comment;
                }
                else if (token->kind == Token_Identifier ||
                         token->kind == Token_Function)
                {
                    if (HasFlag(token->flags, TokenFlag_IsPreprocessor))
                    {
                        foreground = text_preprocessor;
                    }
                    else if (buffer->language)
                    {
                        String string = MakeString(token->length, &buffer->text[token->pos]);
                        StringID id = HashStringID(string);
                        TokenKind kind = GetTokenKindFromStringID(buffer->language, id);
                        if (kind)
                        {
                            foreground = GetThemeColor(TokenThemeIndex(kind));
                            token->kind = kind; // TODO: questionable! deferring the lookup to only do visible areas is good,
                                                // but maybe it should be moved outside of the actual drawing function
                        }
                    }
                }

                if (pos >= token->pos &&
                    editor->pos_at_mouse >= token->pos &&
                    editor->pos_at_mouse <  token->pos + token->length)
                {
                    editor->token_at_mouse = token;
                    background = text_background_highlighted;
                }
            }

            String string = {};
            if (empty_line)
            {
                draw_buffer.Clear();
                draw_buffer.Append(' ');
            }

            int64_t advance          = 1;
            bool right_align         = false;
            bool encountered_newline = false;
            int64_t real_col         = col - left_col;

            int64_t newline_length = PeekNewline(buffer, pos);
            if (newline_length)
            {
                advance = newline_length;
                encountered_newline = true;
            }

            if (b == '\t')
            {
                if (visualize_whitespace)
                {
                    string = ">"_str;
                    foreground = text_foreground_dimmer;
                }
            }
            else if (b == '\r' || b == '\n')
            {
                if (visualize_newlines)
                {
                    if (b == '\r' && ReadBufferByte(buffer, pos + 1) == '\n')
                    {
                        string = "\\r\\n"_str;
                        advance = 2;
                    }
                    else if (b == '\r')
                    {
                        string = "\\r"_str;
                    }
                    else if (b == '\n')
                    {
                        string = "\\n"_str;
                    }
                    else
                    {
                        INVALID_CODE_PATH;
                    }
                    foreground = text_foreground_dimmest;
                    right_align = right_align_visualized_newlines;
                }
            }
            else if (b == ' ' && visualize_whitespace)
            {
                if (pos < indentation_end &&
                    (ReadBufferByte(buffer, pos - 1) == ' ' ||
                     ReadBufferByte(buffer, pos + 1) == ' '))
                {
                    string = PushTempStringF(".");
                    foreground = text_foreground_dimmer;
                }
                else
                {
                    string = MakeString(1, &buffer->text[pos]);
                }
            }
            else
            {
                if (IsHeadUtf8Byte(b))
                {
                    ParseUtf8Result unicode = ParseUtf8Codepoint(&buffer->text[pos]);
                    advance = unicode.advance;
                }

                string = MakeString(advance, &buffer->text[pos]);
            }

            if (draw_cursor)
            {
                for (Cursor *cursor = IterateCursors(view->id, buffer->id);
                     cursor;
                     cursor = cursor->next)
                {
                    if (pos == cursor->pos)
                    {
                        if (string.size == 0)
                        {
                            background = text_foreground;
                            string = " "_str;
                        }
                        else
                        {
                            Swap(foreground, background);
                        }
                        break;
                    }
                    else if (draw_selection)
                    {
                        Range inner = SanitizeRange(cursor->selection.inner);
                        Range outer = SanitizeRange(cursor->selection.outer);

                        if (pos >= inner.start &&
                            pos <  inner.end)
                        {
                            background = inner_selection_background;
                            foreground.r = 255 - inner_selection_background.r;
                            foreground.g = 255 - inner_selection_background.g;
                            foreground.b = 255 - inner_selection_background.b;
                        }
                        else if (pos >= outer.start &&
                                 pos <  outer.end)
                        {
                            background = outer_selection_background;
                            foreground.r = 255 - outer_selection_background.r;
                            foreground.g = 255 - outer_selection_background.g;
                            foreground.b = 255 - outer_selection_background.b;
                        }
                    }
                }
            }

            if (right_align)
            {
                col = line_width - string.size;
            }

            if (b == '\t')
            {
                // TODO: tab logic needs to be done properly everywhere!!
                int64_t unaligned_col = col;
                col = left_col + RoundUpNextTabStop(real_col, indent_width);

                // TODO: This looks a bit janktastic when the cursor is on a tab
                string = PushStringSpace(platform->GetTempArena(), col - unaligned_col);
                for (size_t i = 0; i < string.size; i += 1) string.data[i] = ' ';
            }

            if (top_left.y + row >= bounds.max.y)
            {
                return actual_line_height;
            }

            pos += advance;

            if (!draw_buffer.CanFitAppend(string) ||
                encountered_newline ||
                prev_background.u32 != background.u32 ||
                prev_foreground.u32 != foreground.u32)
            {
                if (draw_buffer.size > 0)
                {
                    PushText(Layer_Text, top_left + MakeV2i(prev_col, row), draw_buffer.AsString(), prev_foreground, prev_background);
                    draw_buffer.Clear();
                }

                prev_col = col;

                prev_background = background;
                prev_foreground = foreground;
            }

            draw_buffer.Append(string);
            col += string.size;

            if (encountered_newline)
            {
                prev_col = 0;
                col = 0;
                break;
            }
        }

        row += 1;
    }
    view->visible_range.end = pos;

    if (top_left.y + row < bounds.max.y)
    {
        PushRect(Layer_Text,
                 MakeRect2iMinMax(MakeV2i(bounds.min.x, top_left.y + Max(0, row)),
                                  MakeV2i(bounds.max.x, bounds.max.y)),
                 text_background_unreachable);
    }

    return actual_line_height;
}

function int64_t
DrawView(View *view, bool is_active_window)
{
    Buffer *buffer = GetBuffer(view);
    Rect2i bounds = view->viewport;

    Color text_foreground         = GetThemeColor("text_foreground"_id);
    Color text_background         = GetThemeColor(is_active_window ? "text_background"_id : "text_background_inactive"_id);
    Color filebar_text_foreground = GetThemeColor("filebar_text_foreground"_id);

    uint64_t filebar_text_background_id = "filebar_text_inactive"_id;
    if (is_active_window)
    {
        filebar_text_background_id = "filebar_text_background"_id;
        switch (editor->edit_mode)
        {
            case EditMode_Text:
            {
                filebar_text_background_id = "filebar_text_background_text_mode"_id;
            } break;

            default:
            {
                /* crickets */
            } break;
        }
    }

    Color filebar_text_background = GetThemeColor(filebar_text_background_id);

    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    Rect2i text_bounds = MakeRect2iMinMax(bounds.min, MakeV2i(bounds.max.x, bounds.max.y - 1));
    PushRect(Layer_Text, bounds, text_background);

    int64_t actual_line_height = DrawTextArea2(view, text_bounds, is_active_window);

    String leaf;
    SplitPath(buffer->name, &leaf);

    int64_t filebar_y = bounds.max.y - 1;

    // PushRectOutline(Layer_Text, bounds, text_foreground, text_background);

    PushRect(Layer_Text, MakeRect2iMinMax(MakeV2i(bounds.min.x + 0, filebar_y), MakeV2i(bounds.max.x - 0, filebar_y + 1)), filebar_text_background);

    PushText(Layer_Text, MakeV2i(bounds.min.x, filebar_y),
             PushTempStringF("%hd:%.*s", buffer->id.index, StringExpand(leaf)),
             filebar_text_foreground, filebar_text_background);

    String right_string = PushTempStringF("[%.*s] %lld:%lld", StringExpand(buffer->language->name), loc.line + 1, loc.col);
    PushText(Layer_Text, MakeV2i(bounds.max.x - right_string.size, filebar_y), right_string, filebar_text_foreground, filebar_text_background);

    int64_t scan_line = Clamp(view->scroll_at, 0, buffer->line_data.count);

    if (core_config->show_scrollbar)
    {
        int64_t line_count = Max(1, (int64_t)buffer->line_data.count);
        int64_t bounds_height = GetHeight(bounds) - 1;
        int64_t scrollbar_size = Max(1, bounds_height*bounds_height / line_count);
        int64_t scrollbar_offset = Min(bounds_height - scrollbar_size, scan_line*bounds_height / line_count);

        for (int i = 0; i < scrollbar_size; i += 1)
        {
            PushTile(Layer_Text,
                     MakeV2i(bounds.max.x - 1, bounds.min.y + i + scrollbar_offset),
                     MakeSprite(' ', text_foreground, MakeColor(127, 127, 127)));
        }
    }

    return actual_line_height;
}

