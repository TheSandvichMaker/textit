function void
DrawGlyph(V2i p, String utf8, Color foreground, Color background, TextStyleFlags flags = 0)
{
    render_state->current_layer = (RenderLayer)(render_state->current_layer - 1);
    PushRect(MakeRect2iMinDim(p.x, p.y, 1, 1), background);
    render_state->current_layer = (RenderLayer)(render_state->current_layer + 1);
    PushUnicode(p, utf8, foreground, background, flags);
}

function void
DrawGlyph(V2i p, uint32_t glyph, Color foreground, Color background, TextStyleFlags flags = 0)
{
    render_state->current_layer = (RenderLayer)(render_state->current_layer - 1);
    PushRect(MakeRect2iMinDim(p.x, p.y, 1, 1), background);
    render_state->current_layer = (RenderLayer)(render_state->current_layer + 1);
    PushTile(p, MakeSprite(glyph, foreground, background), flags);
}

function V2i
DrawText(V2i p, String line, Color foreground, Color background, size_t max = 0)
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
            at_p = DrawText(at_p, string, foreground, background, max);
            i += unicode.advance;
        }
        else if (IsPrintableAscii(line.data[i]))
        {
            Sprite sprite = MakeSprite(line.data[i], foreground, background);
            DrawGlyph(at_p, line.data[i], foreground, background);

            at_p.x += 1;
            i += 1;
        }
        else
        {
            String string = PushTempStringF("\\x%02hhx", line.data[i]);
            at_p = DrawText(at_p, string, foreground, background, max);
            i += 1;
        }
    }
    return at_p;
}

function int64_t
DrawTextArea(View *view, Rect2i bounds, bool is_active_window)
{
    Arena *arena = platform->GetTempArena();

    Buffer *buffer = GetBuffer(view);
    Project *project = buffer->project;
    LanguageSpec *language = buffer->language;

    bool draw_cursor    = is_active_window;
    bool draw_selection = (editor->edit_mode != EditMode_Text);

    bool visualize_newlines              = core_config->visualize_newlines;
    bool right_align_visualized_newlines = core_config->right_align_visualized_newlines; (void)right_align_visualized_newlines;
    bool visualize_whitespace            = core_config->visualize_whitespace;
    bool show_line_numbers               = core_config->show_line_numbers;
    int  indent_width                    = core_config->indent_width;

    Color text_comment                   = GetThemeColor("text_comment"_id);
    Color text_preprocessor              = GetThemeColor("text_preprocessor"_id);
    Color text_foreground                = GetThemeColor("text_foreground"_id);
    Color text_foreground_dim            = GetThemeColor("text_foreground_dim"_id);
    Color text_foreground_dimmer         = GetThemeColor("text_foreground_dimmer"_id);
    Color text_foreground_dimmest        = GetThemeColor("text_foreground_dimmest"_id);
    Color text_background                = GetThemeColor(is_active_window ? "text_background"_id : "text_background_inactive"_id);
    Color text_background_unreachable    = GetThemeColor("text_background_unreachable"_id);
    Color text_background_highlighted    = GetThemeColor("text_background_highlighted"_id);
    Color text_search_highlight          = GetThemeColor("text_search_highlight"_id);
    Color text_nest_highlight_foreground = GetThemeColor("text_nest_highlight_foreground"_id);
    Color text_nest_highlight_background = GetThemeColor("text_nest_highlight_background"_id);
    Color unrenderable_text_foreground   = GetThemeColor("unrenderable_text_foreground"_id);
    Color unrenderable_text_background   = GetThemeColor("unrenderable_text_background"_id);
    Color inner_selection_background     = GetThemeColor("inner_selection_background"_id);
    Color outer_selection_background     = GetThemeColor("outer_selection_background"_id);
    Color line_highlight                 = GetThemeColor("line_highlight"_id);

    V2i top_left = bounds.min;

    int64_t buffer_line_count = GetLineCount(buffer);

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
        PushLayer(Layer_ViewBackground);
        PushRect(MakeRect2iMinDim(bounds.min.x, bounds.min.y, bounds.max.x, top_left.y + row), text_background_unreachable);
        PushLayer(Layer_ViewForeground);
        for (int64_t y = bounds.min.y; y < top_left.y + row; y += 1)
        {
            DrawGlyph(MakeV2i(top_left.x, y), '~', text_foreground_dimmer, text_background);
        }
        actual_line_height += -view->scroll_at;
    }

    int64_t line = min_line;

    LineIndexIterator line_it = IterateLineIndexFromLine(buffer, min_line);

    LineInfo info;
    GetLineInfo(&line_it, &info);

    TokenIterator token_it = IterateLineTokens(&info);

    int nest_highlight_count = 0;
    Range *nest_highlights = PushArrayNoClear(arena, 64, Range);

    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        TokenIterator nest_it = IterateTokens(buffer, cursor->pos);

        if (GetOtherNestTokenKind(nest_it.token.kind) && IsInTokenRange(nest_it.token, cursor->pos))
        {
            Direction direction = (IsNestOpener(nest_it.token.kind) ? Direction_Forward : Direction_Backward);

            NestHelper nests = {};
            if (IsInNest(&nests, nest_it.token.kind, direction) == NestResult_EnteringNest)
            {
                Token start_token = nest_it.token;
                Advance(&nest_it, direction);

                for (; IsValid(&nest_it); Advance(&nest_it, direction))
                {
                    if (IsInNest(&nests, nest_it.token.kind, direction) == NestResult_ExitingNest)
                    {
                        Token end_token = nest_it.token;
                        if (nest_highlight_count < 64) nest_highlights[nest_highlight_count++] = GetTokenRange(start_token);
                        if (nest_highlight_count < 64) nest_highlights[nest_highlight_count++] = GetTokenRange(end_token);
                        break;
                    }
                }
            }
        }
    }

    MergeSort(nest_highlight_count, nest_highlights, +[](Range *a, Range *b) { return a->start >= b->start; });

    int64_t pos = line_it.range.start;
    view->visible_range.start = pos;

    V2i metrics = editor->font_metrics;

    Range search_highlight = FindNextOccurrence(buffer, pos, editor->search.as_string, editor->search_flags);

    while (IsInBufferRange(buffer, pos))
    {
        if (pos >= line_it.range.end)
        {
            Next(&line_it);
        }

        if (!IsValid(&line_it))
        {
            break;
        }

        actual_line_height += 1;

        bool empty_line = (ReadBufferByte(buffer, line_it.range.start) == '\r' ||
                           ReadBufferByte(buffer, line_it.range.start) == '\n');
        int64_t indentation_end = FindFirstNonHorzWhitespace(buffer, line_it.range.start);

        Color base_background = text_background;

        bool contains_cursor = false;
        if (draw_cursor)
        {
            for (Cursor *cursor = IterateCursors(view->id, buffer->id);
                 cursor;
                 cursor = cursor->next)
            {
                if (cursor->pos >= line_it.range.start && cursor->pos < line_it.range.end)
                {
                    contains_cursor = true;
                }
            }
        }

        if (contains_cursor)
        {
            base_background = line_highlight;
        }

        int64_t start_row = row;

        int64_t col = 0;
        int64_t line_number = line + 1;

        if (show_line_numbers)
        {
            DrawText(top_left + MakeV2i(col, row), PushTempStringF("%-4lld", line_number), text_foreground_dimmest, base_background);
            col += Max(5, max_line_digits + 1);
            // prev_col = col;
        }

        int64_t left_col = col;

        while (IsInBufferRange(buffer, pos))
        {
            if (pos >= search_highlight.end)
            {
                search_highlight = FindNextOccurrence(buffer, search_highlight.start + 1, editor->search.as_string, editor->search_flags);
            }

            uint8_t b = ReadBufferByte(buffer, pos);

            Color foreground = text_foreground;
            Color background = base_background;
            TextStyleFlags text_style = 0;

            //
            // Colorize Tokens
            //

#if 1
            Token token = token_it.token;

            while (token &&
                   (pos >= (token.pos + token.length)))
            {
                token = Next(&token_it);
            }
#else
            Token token = GetTokenAt(buffer, pos);
#endif

            if (token)
            {
                String token_name = MakeString(token.length, &buffer->text[token.pos]);

                StringID foreground_id = "text_foreground"_id;
                if (core_config->syntax_highlighting)
                {
                    foreground_id = GetThemeIDForToken(language, &token);
                    // FIXME: Decide what to do about highlighting untagged functions
                    if (token.kind == Token_Function)
                    {
                        foreground_id = "text_unknown_function"_id;
                    }

                    if (token.kind == Token_Identifier ||
                        token.kind == Token_Function)
                    {
                        String string = MakeString(token.length, &buffer->text[token.pos]);
                        StringID id = HashStringID(string);
                        TokenKind kind = GetTokenKindFromStringID(language, id);
                        if (kind)
                        {
                            foreground_id = GetThemeIDForToken(language, &token);
                            token.kind = kind; // TODO: questionable! deferring the lookup to only do visible areas is good,
                                                // but maybe it should be moved outside of the actual drawing function
                        }

                        ScopedMemory temp;
                        // TODO: Bit excessive
                        for (Tag *tag = PushTagsWithName(temp, project, token_name); tag; tag = tag->next)
                        {
                            if (tag->related_token_kind == token.kind)
                            {
                                if (tag->kind == Tag_CommentAnnotation)
                                {
                                    if (HasFlag(token.flags, TokenFlag_IsComment))
                                    {
                                        foreground_id = GetThemeIDForTag(language, tag);
                                    }
                                }
                                else
                                {
                                    foreground_id = GetThemeIDForTag(language, tag);
                                }
                                break;
                            }
                        }
                    }

                    if (HasFlag(token.flags, TokenFlag_IsComment))
                    {
                        if (foreground_id != "text_comment_annotation"_id)
                        {
                            foreground_id = "text_comment"_id;
                            if (AreEqual(token_name, "NOTE"_str, StringMatch_CaseInsensitive))
                            {
                                foreground_id = "text_comment_note"_id;
                            }
                            else if (AreEqual(token_name, "TODO"_str, StringMatch_CaseInsensitive))
                            {
                                foreground_id = "text_comment_todo"_id;
                            }
                            else if (AreEqual(token_name, "FIXME"_str, StringMatch_CaseInsensitive))
                            {
                                foreground_id = "text_comment_fixme"_id;
                            }
                        }
                    }
                    else if (HasFlag(token.flags, TokenFlag_PartOfString))
                    {
                        if (foreground_id != "text_string_special"_id)
                        {
                            foreground_id = "text_string"_id;
                        }
                    }
                    else if (HasFlag(token.flags, TokenFlag_IsPreprocessor))
                    {
                        foreground_id = "text_preprocessor"_id;
                    }
                }

                foreground = GetThemeColor(foreground_id);
                text_style = GetThemeStyle(foreground_id);

                if (token.flags & TokenFlag_NonParticipating)
                {
                    foreground.r = foreground.r / 2;
                    foreground.g = foreground.g / 2;
                    foreground.b = foreground.b / 2;
                }
            }

            String string = {};

            int64_t advance             = 1;
            bool    right_align         = false;
            bool    encountered_newline = false;
            int64_t real_col            = col - left_col;

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
                    string = ">"_str; // NOTE: gets overwritten down the line
                    foreground = text_foreground_dimmer;
                    text_style = 0;
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
                    text_style = 0;
                }
                else if (empty_line)
                {
                    string = " "_str;
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
                    text_style = 0;
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

            if (editor->show_search_highlight && IsInRange(search_highlight, pos))
            {
                background = text_search_highlight;
            }

            bool nest_highlight = false;
            if (nest_highlight_count > 0)
            {
                Range highlight_range = nest_highlights[nest_highlight_count - 1];
                if (IsInRange(highlight_range, pos))
                {
                    nest_highlight = true;
                    foreground = text_nest_highlight_foreground;
                    background = text_nest_highlight_background;
                    if (pos + 1 == highlight_range.end)
                    {
                        nest_highlight_count -= 1;
                    }
                }
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
                            if (!nest_highlight)
                            {
                                foreground.r = 255 - inner_selection_background.r;
                                foreground.g = 255 - inner_selection_background.g;
                                foreground.b = 255 - inner_selection_background.b;
                            }
                        }
                        else if (pos >= outer.start &&
                                 pos <  outer.end)
                        {
                            background = outer_selection_background;
                            if (!nest_highlight)
                            {
                                foreground.r = 255 - outer_selection_background.r;
                                foreground.g = 255 - outer_selection_background.g;
                                foreground.b = 255 - outer_selection_background.b;
                            }
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
                int64_t   aligned_col = left_col + RoundUpNextTabStop(real_col, indent_width);

                // TODO: This looks a bit janktastic when the cursor is on a tab
                string = PushStringSpace(platform->GetTempArena(), aligned_col - unaligned_col);
                for (size_t i = 0; i < string.size; i += 1) string.data[i] = ' ';
                if (visualize_whitespace) string.data[0] = '>';
            }

            if (top_left.y + row >= bounds.max.y)
            {
                return actual_line_height;
            }

            pos += advance;

            for (size_t i = 0; i < string.size; i += 1)
            {
                uint32_t glyph_size = 1;
                if (IsHeadUtf8Byte(b))
                {
                    ParseUtf8Result unicode = ParseUtf8Codepoint(&string.data[i]);
                    glyph_size = unicode.advance;
                    i += unicode.advance - 1;
                }
                String glyph = MakeString(glyph_size, &string.data[i]);
                DrawGlyph(top_left + MakeV2i(col, row), glyph, foreground, background, text_style);
                col += 1;
                if (col + 1 >= line_width)
                {
                    DrawGlyph(top_left + MakeV2i(col, row), '\\', text_foreground_dimmer, background);
                    col = 0;
                    row += 1;
                }
            }

            if (encountered_newline)
            {
                col = 0;
                break;
            }
        }

        PushLayer(Layer_ViewBackground);

        PushRect(MakeRect2iMinMax(MakeV2i(bounds.min.x, top_left.y + start_row), 
                                  MakeV2i(bounds.max.x, top_left.y + row + 1)), 
                 base_background);

        PushLayer(Layer_ViewForeground);

        row += 1;
    }
    view->visible_range.end = pos;

    if (top_left.y + row < bounds.max.y)
    {
        PushLayer(Layer_ViewBackground);
        PushRect(MakeRect2iMinDim(bounds.min.x, top_left.y + row, bounds.max.x, bounds.max.y), text_background_unreachable);
        PushLayer(Layer_ViewForeground);
        for (int64_t y = top_left.y + row; y < bounds.max.y; y += 1)
        {
            DrawGlyph(MakeV2i(top_left.x, y), '~', text_foreground_dimmer, text_background);
        }
        actual_line_height += bounds.max.y - (top_left.y + row) + 1;
    }

    return actual_line_height;
}

function int64_t
DrawView(View *view, bool is_active_window)
{
    Buffer *buffer = GetBuffer(view);
    Rect2i bounds = view->viewport;

    PushLayer(Layer_ViewForeground);

    Color text_foreground          = GetThemeColor("text_foreground"_id);
    Color text_background          = GetThemeColor(is_active_window ? "text_background"_id : "text_background_inactive"_id);
    Color filebar_text_foreground  = GetThemeColor("filebar_text_foreground"_id);
    Color filebar_text_highlighted = GetThemeColor("filebar_text_highlighted"_id);

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

    int64_t actual_line_height = DrawTextArea(view, text_bounds, is_active_window);
    int64_t line = loc.line + 1;
    int64_t line_count = Max(1, GetLineCount(buffer));
    int64_t line_percentage = 100*line / line_count;

    String leaf;
    SplitPath(buffer->name, &leaf);

    int64_t filebar_y = bounds.max.y - 1;

    PushLayer(Layer_ViewBackground);
    PushRect(MakeRect2iMinMax(MakeV2i(bounds.min.x + 0, filebar_y), MakeV2i(bounds.max.x - 0, filebar_y + 1)), filebar_text_background);

    PushLayer(Layer_ViewForeground);
    V2i left_at = DrawText(MakeV2i(bounds.min.x, filebar_y),
                           PushTempStringF("%hd:%.*s", buffer->id.index, StringExpand(leaf)),
                           filebar_text_foreground, filebar_text_background);

    if (buffer->last_save_undo_ordinal != buffer->undo.current_ordinal)
    {
        DrawText(left_at, "[+]"_str, filebar_text_foreground, filebar_text_background);
    }

    String right_string = PushTempStringF("[%.*s] %lld%% %lld:%lld ", StringExpand(buffer->language->name), line_percentage, loc.line + 1, loc.col);
#if TEXTIT_INTERNAL
    right_string = PushTempStringF("*DEBUG BUILD* %.*s", StringExpand(right_string));
#endif

    int cursor_count = 0;
    for (Cursor *it_cursor = IterateCursors(view); it_cursor; it_cursor = it_cursor->next)
    {
        cursor_count += 1;
    }
    
    if (cursor_count > 1)
    {
        right_string = PushTempStringF("multi-cursor: #%d %.*s", cursor_count, StringExpand(right_string));
    }

    DrawText(MakeV2i(bounds.max.x - right_string.size, filebar_y), right_string, filebar_text_foreground, filebar_text_background);

    if (core_config->show_scrollbar)
    {
        int64_t scan_line = Clamp(view->scroll_at, 0, GetLineCount(buffer));
        int64_t bounds_height = GetHeight(bounds) - 1;
        int64_t scrollbar_size = Max(1, bounds_height*bounds_height / line_count);
        int64_t scrollbar_offset = Min(bounds_height - scrollbar_size, scan_line*bounds_height / line_count);

        for (int i = 0; i < scrollbar_size; i += 1)
        {
            DrawGlyph(MakeV2i(bounds.max.x - 1, bounds.min.y + i + scrollbar_offset), ' ', text_foreground, MakeColor(127, 127, 127));
        }
    }

    if (core_config->debug_show_line_index)
    {
        int64_t at_line = Clamp(view->scroll_at, 0, GetLineCount(buffer));
        for (int64_t y = bounds.min.y; y < bounds.max.y; y += 1)
        {
            LineInfo info;
            FindLineInfoByLine(buffer, at_line, &info);

            at_line += 1;

            DrawText(MakeV2i(bounds.max.x - 32, y), 
                     PushTempStringF("%lld: { %lld, %lld }", info.line, info.range.start, info.range.end),
                     COLOR_WHITE, COLOR_BLACK);
        }
    }

    if (core_config->debug_show_jump_history)
    {
        DrawText(MakeV2i(bounds.max.x - 32, bounds.min.x),
                 PushTempStringF("jump at: %d, jump top: %d", view->jump_at, view->jump_top),
                 COLOR_WHITE, COLOR_BLACK);
        for (uint32_t i = OldestJumpIndex(view); i < view->jump_top; i += 1)
        {
            Jump *jump = GetJump(view, i);
            V2i p = MakeV2i(bounds.max.x - 32, bounds.min.x + i + 1);
            Color color = COLOR_WHITE;
            if (view->jump_at == i) color = MakeColor(255, 0, 0);
            DrawText(p, PushTempStringF("[%d] buffer: %d, pos: %lld", i, jump->buffer.index, jump->pos), color, COLOR_BLACK);
        }
    }

    return actual_line_height;
}

function void
DrawCommandLines()
{
    PushLayer(Layer_OverlayForeground);

    Color text_foreground           = GetThemeColor("command_line_foreground"_id);
    Color text_background           = GetThemeColor("command_line_background"_id);
    Color color_name                = GetThemeColor("command_line_name"_id);
    Color color_option              = GetThemeColor("command_line_option"_id);
    Color color_selected            = GetThemeColor("command_line_option_selected"_id);
    Color color_numbers             = GetThemeColor("command_line_option_numbers"_id);
    Color color_numbers_highlighted = GetThemeColor("command_line_option_numbers_highlighted"_id);
    Color overlay_background        = GetThemeColor("command_line_background"_id);

    V2i p = MakeV2i(render_state->viewport.min.x, render_state->viewport.max.y - 1);

    V2i text_p = p;

    Arena *arena = platform->GetTempArena();

    StringList name_list = {};
    for (int i = 0; i < editor->command_line_count; i += 1)
    {
        CommandLine *cl = editor->command_lines[i];
        PushString(&name_list, arena, cl->name);
    }
    String names = PushFlattenedString(&name_list, arena, " > "_str, StringSeparator_AfterLast);

    text_p = DrawText(text_p, names, color_name, text_background);

    CommandLine *cl = editor->command_lines[editor->command_line_count - 1];

    int64_t total_width  = 0;
    int64_t total_height = cl->prediction_count + cl->prediction_overflow;

    if (cl->highlight_numbers)
    {
        color_numbers = color_numbers_highlighted;
    }

    Prediction *active_prediction = nullptr;

    if (cl->prediction_count > 0)
    {
        active_prediction = GetPrediction(cl, 0);

        int prediction_offset = 1;
        for (int i = 0; i < cl->prediction_count; i += 1)
        {
            Prediction *prediction = GetPrediction(cl, i);
            String text = prediction->preview_text;

            Color color = GetThemeColor(prediction->color ? prediction->color : "command_line_option"_id);
            if (i == cl->prediction_selected_index)
            {
                active_prediction = prediction;
                color = color_selected;
            }

            if (prediction->quickselect_char)
            {
                DrawText(p + MakeV2i(0, -prediction_offset), PushTempStringF("%c ", prediction->quickselect_char), color_numbers, overlay_background);
            }
            else if (i < 9 + 26)
            {
                int c = (i < 9 ? '1' + i : 'A' + i - 9);
                DrawText(p + MakeV2i(0, -prediction_offset), PushTempStringF("%c ", c), color_numbers, overlay_background);
            }

            DrawText(p + MakeV2i(2, -prediction_offset), text, color, overlay_background);

            total_width = Max(total_width, 2 + (int64_t)text.size);

            prediction_offset += 1;
        }
        
        if (cl->prediction_overflow)
        {
            DrawText(p + MakeV2i(2, -prediction_offset), "... and more"_str, color_option, overlay_background);
        }
    }

    PushLayer(Layer_OverlayBackground);

    PushRect(MakeRect2iMinDim(p + MakeV2i(0, -total_height), MakeV2i(total_width, total_height)), overlay_background);

    PushLayer(Layer_OverlayForeground);

    int cursor_at = cl->cursor;
    String text = MakeString(cl->count, cl->text); if (cl->prediction_selected_index != -1)
    {
        text = GetPrediction(cl, cl->prediction_selected_index)->text;
        cursor_at = (int)text.size;
    }

    DrawText(text_p, text, text_foreground, text_background);
    DrawGlyph(text_p + MakeV2i(cursor_at, 0), ' ', text_background, text_foreground);

    if (active_prediction)
    {
        Command *cmd = FindCommand(active_prediction->text, StringMatch_CaseInsensitive);
        if (cmd != NullCommand())
        {
            String desc = cmd->description;
            DrawText(MakeV2i(render_state->viewport.max.x - desc.size, text_p.y), desc, text_foreground, text_background);
        }
    }
}
