COMMAND_PROC(ReportMetrics,
             "Report some relevant metrics for development"_str)
{
    Buffer *buffer = GetActiveBuffer();
    platform->DebugPrint("Metrics for buffer %.*s:\n", StringExpand(buffer->name));

    platform->DebugPrint("\tBuffer count: %lld, line count: %u, token count: %u\n",
                         buffer->count, buffer->line_data.count, buffer->tokens.count);

    size_t text_bytes  = (size_t)buffer->count;
    size_t line_bytes  = sizeof(LineData)*(size_t)buffer->line_data.count; 
    size_t token_bytes = sizeof(Token)*(size_t)buffer->tokens.count;       
    size_t arena_bytes = buffer->arena.used;

    platform->DebugPrint("\tText memory: %s, line memory: %s, token memory: %s, buffer arena: %s, total: %s\n",
                         FormatHumanReadableBytes(text_bytes).data,
                         FormatHumanReadableBytes(line_bytes).data,
                         FormatHumanReadableBytes(token_bytes).data,
                         FormatHumanReadableBytes(arena_bytes).data,
                         FormatHumanReadableBytes(text_bytes + line_bytes + token_bytes + arena_bytes).data);

    platform->DebugPrint("\tLine memory ratio: %f, token memory ratio: %f\n",
                         (double)line_bytes / (double)text_bytes,
                         (double)token_bytes / (double)text_bytes);
}

COMMAND_PROC(TestLineSearches,
             "Test optimized pos -> line functions against reference implementation"_str)
{
    Buffer *buffer = GetActiveBuffer();

    RandomSeries entropy = MakeRandomSeries(0xDEADBEEF);

    size_t run_count = 1000;
    for (size_t i = 0; i < run_count; i += 1)
    {
        int64_t pos = (int64_t)RandomChoice(&entropy, (uint32_t)buffer->count);
        BufferLocation reference_loc = CalculateBufferLocationFromPosLinearSearch(buffer, pos);
        BufferLocation bs_loc        = CalculateBufferLocationFromPos(buffer, pos);
        BufferLocation o1_loc        = CalculateBufferLocationFromPosO1Search(buffer, pos);
        Assert(reference_loc.line == bs_loc.line);
        Assert(reference_loc.line == o1_loc.line);
    }
    platform->DebugPrint("Success\n");
}

COMMAND_PROC(BenchmarkPosToLine,
             "Run a benchmark for the pos -> line function"_str)
{
    Buffer *buffer = GetActiveBuffer();

    int64_t orig_line_count = buffer->line_data.count;
    int64_t orig_buff_count = buffer->count;

    static volatile int64_t dont_optimize_me;

    RandomSeries entropy = MakeRandomSeries(0xDEADBEEF);

    int64_t divisor = 128;
    while (divisor)
    {
        int64_t test_line_count = orig_line_count / divisor;
        int64_t test_buff_count = buffer->line_data[test_line_count - 1].range.end;

        buffer->line_data.count = (unsigned)test_line_count;
        buffer->count           = test_buff_count;

        platform->DebugPrint("Running pos to line benchmark, buffer size: %lld, line count: %lld\n",
                             buffer->count, buffer->line_data.count);

        {
            unsigned int dummy;
            uint64_t clocks = 0;

            size_t run_count = 32000;
            for (size_t i = 0; i < run_count; i += 1)
            {
                uint64_t start = __rdtscp(&dummy);
                int64_t pos = (int64_t)RandomChoice(&entropy, (uint32_t)buffer->count);
                BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);
                dont_optimize_me = loc.line;
                uint64_t end = __rdtscp(&dummy);
                clocks += end - start;
            }

            platform->DebugPrint("\tBinary search ran %zu tests in %f megacycles, %llu cycles per test\n",
                                 run_count, (double)clocks / 1000000.0, clocks / run_count);
        }

        {
            unsigned int dummy;
            uint64_t clocks = 0;

            size_t run_count = 32000;
            for (size_t i = 0; i < run_count; i += 1)
            {
                uint64_t start = __rdtscp(&dummy);
                int64_t pos = (int64_t)RandomChoice(&entropy, (uint32_t)buffer->count);
                BufferLocation loc = CalculateBufferLocationFromPosO1Search(buffer, pos);
                dont_optimize_me = loc.line;
                uint64_t end = __rdtscp(&dummy);
                clocks += end - start;
            }

            platform->DebugPrint("\tO(1) search ran %zu tests in %f megacycles, %llu cycles per test\n\n",
                                 run_count, (double)clocks / 1000000.0, clocks / run_count);
        }

        divisor >>= 1;
    }

    buffer->line_data.count = (unsigned)orig_line_count;
    buffer->count           = orig_buff_count;
}

COMMAND_PROC(NextBuffer,
             "Select the next buffer"_str)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    BufferID next_buffer = {};
    for (size_t index = 1; index < editor->buffer_count; index += 1)
    {
        BufferID id = editor->used_buffer_ids[index];
        if (id == buffer->id)
        {
            if (index + 1 == editor->buffer_count)
            {
                next_buffer = editor->used_buffer_ids[1];
            }
            else
            {
                next_buffer = editor->used_buffer_ids[index + 1];
            }
        }
    }

    if (next_buffer)
    {
        view->next_buffer = next_buffer;
    }
}

COMMAND_PROC(ForceTick,
             "Force the editor to tick"_str)
{
    platform->PushTickEvent();
}

COMMAND_PROC(LoadDefaultIndentRules,
             "Load the default indent rules for the current buffer"_str)
{
    LoadDefaultIndentRules(&editor->default_indent_rules);
}

COMMAND_PROC(LoadOtherIndentRules,
             "Load the other indent rules for the current buffer"_str)
{
    LoadOtherIndentRules(&editor->default_indent_rules);
}

COMMAND_PROC(EnterCommandLineMode)
{
    CommandLine *cl = BeginCommandLine();

    cl->GatherPredictions = [](CommandLine *cl)
    {
        if (cl->count == 0) return;

        cl->predictions = PushArray(cl->arena, command_list->command_count, String);

        String command_string = MakeString(cl->count, cl->text);
        for (size_t i = 0; i < command_list->command_count; i += 1)
        {
            Command *command = &command_list->commands[i];
            if ((command->kind == Command_Basic) &&
                (command->flags & Command_Visible) &&
                MatchPrefix(command->name, command_string, StringMatch_CaseInsensitive))
            {
                cl->predictions[cl->prediction_count++] = command->name;
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String command_string = MakeString(cl->count, cl->text);
        Command *command = FindCommand(command_string, StringMatch_CaseInsensitive);
        if (command &&
            (command->kind == Command_Basic) &&
            (command->flags & Command_Visible))
        {
            command->command();
        }
    };
}

COMMAND_PROC(OpenBuffer,
             "Interactively open a buffer"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Open Buffer"_str;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        cl->predictions = PushArray(cl->arena, editor->buffer_count, String);

        String string = MakeString(cl->count, cl->text);
        for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
        {
            Buffer *buffer = it.buffer;
            if (string.size == 0 ||
                MatchPrefix(buffer->name, string, StringMatch_CaseInsensitive))
            {
                cl->predictions[cl->prediction_count++] = buffer->name;
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = MakeString(cl->count, cl->text);
        for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
        {
            Buffer *buffer = it.buffer;
            if (AreEqual(buffer->name, string))
            {
                View *view = GetActiveView();
                view->next_buffer = buffer->id;
                break;
            }
        }
    };
}

COMMAND_PROC(Exit, "Exit the editor"_str)
{
    platform->exit_requested = true;
}

COMMAND_PROC(qa, "Exit the editor"_str)
{
    platform->exit_requested = true;
}

COMMAND_PROC(ToggleVisualizeNewlines, "Toggle the visualization of newlines"_str)
{
    core_config->visualize_newlines = !core_config->visualize_newlines;
}

COMMAND_PROC(ToggleVisualizeWhitespace, "Toggle the visualization of whitespaces"_str)
{
    core_config->visualize_whitespace = !core_config->visualize_whitespace;
}

COMMAND_PROC(ToggleLineNumbers, "Toggle the line number display"_str)
{
    core_config->show_line_numbers = !core_config->show_line_numbers;
}

COMMAND_PROC(EnterTextMode,
             "Enter Text Input Mode"_str)
{
    editor->next_edit_mode = EditMode_Text;
    BeginUndoBatch(GetActiveBuffer());
}

COMMAND_PROC(Append)
{
    View *view = GetActiveView();
    Cursor *cursor = GetCursor(view);
    cursor->pos += 1;
    CMD_EnterTextMode();
}

COMMAND_PROC(AppendAtEnd)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);
    cursor->pos = FindLineEnd(buffer, cursor->pos).inner + 1;
    CMD_EnterTextMode();
}

COMMAND_PROC(EnterCommandMode,
             "Enter Command Mode"_str)
{
    editor->next_edit_mode = EditMode_Command;

    Buffer *buffer = GetActiveBuffer();
    EndUndoBatch(buffer);

    View *view = GetActiveView();
    Cursor *cursor = GetCursor(view);

    BufferLocation loc = CalculateRelativeMove(buffer, cursor, MakeV2i(-1, 0));
    cursor->pos = loc.pos;
    cursor->selection = MakeSelection(cursor->pos);
    cursor->sticky_col = loc.col;
}

COMMAND_PROC(CenterView,
             "Center the view around the cursor"_str)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    view->scroll_at = loc.line - viewport_height / 2;
}

COMMAND_PROC(JumpToBufferStart, "Jump to the start of the buffer"_str)
{
    View *view = GetActiveView();
    SetCursor(view, 0);
}

COMMAND_PROC(JumpToBufferEnd, "Jump to the end of the buffer"_str)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    SetCursor(view, buffer->count - 1);
}

MOVEMENT_PROC(MoveLeft)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos - 1, line_range);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);
    cursor->sticky_col = loc.col;

    return MakeMove(pos);
}

MOVEMENT_PROC(MoveRight)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos + 1, line_range);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);
    cursor->sticky_col = loc.col;

    return MakeMove(pos);
}

MOVEMENT_PROC(MoveLeftIdentifier)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Selection selection = ScanWordBackward2(buffer, pos);

    Move result = {};
    result.pos = selection.outer.end;
    result.selection = selection;

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.pos);
    cursor->sticky_col = loc.col;

    return result;
}

MOVEMENT_PROC(MoveRightIdentifier)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Selection selection = ScanWordForward2(buffer, pos);

    Move result = {};
    result.pos = selection.outer.end;
    result.selection = selection;

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.pos);
    cursor->sticky_col = loc.col;

    return result;
}

MOVEMENT_PROC(MoveLineStart)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 0;

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(MakeRange(pos, line_range.start));
}

MOVEMENT_PROC(MoveLineEnd)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 9999;

    int64_t pos = cursor->pos;
    auto [inner, outer] = FindLineEnd(buffer, pos);

    Move move = {};
    move.selection.inner.start = pos;
    move.selection.outer.start = pos;
    move.selection.inner.end   = inner;
    move.selection.outer.end   = outer;
    move.pos                   = inner;

    return move;
}

MOVEMENT_PROC(EncloseLine)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(line_range);
}

MOVEMENT_PROC(EncloseNextScope)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range result = MakeRange(pos);

    TokenIterator it = SeekTokenIterator(buffer, pos);

    TokenKind opening_kind = Token_None;
    TokenKind closing_kind = Token_None;
    bool seek_forward = true;
    int depth = 0;
    while (IsValid(&it))
    {
        Token *t = Next(&it);
        if ((t->kind == Token_LeftParen) ||
            (t->kind == Token_LeftScope) ||
            (t->kind == Token_RightParen) ||
            (t->kind == Token_RightScope))
        {
            opening_kind = t->kind;
            if (opening_kind == Token_LeftParen)  { seek_forward = true;  closing_kind = Token_RightParen; }
            if (opening_kind == Token_LeftScope)  { seek_forward = true;  closing_kind = Token_RightScope; }
            if (opening_kind == Token_RightParen) { seek_forward = false; closing_kind = Token_LeftParen; Prev(&it); }
            if (opening_kind == Token_RightScope) { seek_forward = false; closing_kind = Token_LeftScope; Prev(&it); }

            depth = 1;
            break;
        }
    }
    if (opening_kind)
    {
        while (IsValid(&it))
        {
            Token *t = (seek_forward ? Next(&it) : Prev(&it));
            if (t->kind == opening_kind)
            {
                depth += 1;
            }
            else if (t->kind == closing_kind)
            {
                depth -= 1;
                if (depth <= 0)
                {
                    result.end = (seek_forward ? t->pos + t->length : t->pos);
                    break;
                }
            }
        }
    }

    return MakeMove(result);
}

function Move
SelectSurroundingNest(View *view,
                      TokenKind open_nest, TokenKind close_nest,
                      bool line_selection)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move result = {};
    result.flags     = MoveFlag_NoAutoRepeat;
    result.pos       = cursor->pos;
    result.selection = cursor->selection;
    
    int64_t pos = cursor->pos;

    int64_t start_pos       = -1;
    int64_t inner_start_pos = -1;

    TokenIterator it = SeekTokenIterator(buffer, pos);

    int depth = 0;
    while (IsValid(&it))
    {
        Token *t = Prev(&it);
        if (!t) break;

        if (t->kind == close_nest)
        {
            depth += 1;
        }
        if (t->kind == open_nest)
        {
            depth -= 1;
            if (depth < 0)
            {
                start_pos = t->pos;
                inner_start_pos = t->pos + t->length;
                break;
            }
        }
    }

    if (start_pos >= 0)
    {
        while (IsValid(&it))
        {
            Token *t = Next(&it);
            if (!t) break;

            if (t->kind == open_nest)
            {
                depth += 1;
            }
            else if (t->kind == close_nest)
            {
                depth -= 1;
                if (depth < 0)
                {
                    int64_t end_pos       = t->pos + t->length;
                    int64_t inner_end_pos = t->pos;

                    result.selection.inner.start = inner_start_pos;
                    result.selection.inner.end   = inner_end_pos;
                    result.selection.outer.start = start_pos;
                    result.selection.outer.end   = end_pos;

                    if (line_selection)
                    {
                        int64_t start_line = GetLineNumber(buffer, result.selection.outer.start);
                        int64_t end_line   = GetLineNumber(buffer, result.selection.outer.end);

                        int64_t inner_start_line = start_line + 1;
                        int64_t inner_end_line   = Max(start_line, end_line - 1);

                        if (start_line != end_line)
                        {
                            result.selection.outer.start = GetLineRange(buffer, start_line).start;
                            result.selection.outer.end   = GetLineRange(buffer, end_line).end;
                        }

                        if (inner_start_line != inner_end_line)
                        {
                            result.selection.inner.start = GetInnerLineRange(buffer, inner_start_line).start;
                            result.selection.inner.end   = GetInnerLineRange(buffer, inner_end_line).end;
                        }

                    }

                    result.pos = result.selection.outer.start;

                    break;
                }
            }
        }
    }

    return result;
}

MOVEMENT_PROC(EncloseSurroundingScope)
{
    View *view = GetActiveView();
    return SelectSurroundingNest(view, Token_LeftScope, Token_RightScope, true);
}

MOVEMENT_PROC(EncloseSurroundingParen)
{
    View *view = GetActiveView();
    return SelectSurroundingNest(view, Token_LeftParen, Token_RightParen, false);
}

MOVEMENT_PROC(EncloseParameter)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move result = {};
    result.pos             = cursor->pos;
    result.selection.inner = cursor->selection.inner;
    result.selection.outer = cursor->selection.outer;
    
    int64_t pos = cursor->pos;

    Token *alternative_start = nullptr;
    TokenKind end_kind      = 0;
    int64_t   end_pos       = -1;
    int64_t   inner_end_pos = -1;

    NestHelper nests = {};
    TokenIterator it = SeekTokenIterator(buffer, pos);

    while (IsValid(&it) || alternative_start)
    {
        if (!IsValid(&it) && alternative_start)
        {
            Clear(&nests);
            Rewind(&it, alternative_start);
            alternative_start = nullptr;
        }

        Token *t = Next(&it);
        if (!t) break;

        if (!alternative_start &&
            t->kind == Token_LeftParen)
        {
            alternative_start = PeekNext(&it);
        }

        if (IsInNest(&nests, t->kind, Direction_Forward))
        {
            continue;
        }

        if (t->kind == Token_RightParen ||
            t->kind == ',')
        {
            end_kind      = t->kind;
            inner_end_pos = t->pos;
            end_pos       = t->pos;
            if (t->kind == ',')
            {
                end_pos = t->pos + t->length;
                Token *next = PeekNext(&it);
                if (next)
                {
                    end_pos = next->pos;
                }
            }
            break;
        }
    }

    if (end_pos >= 0)
    {
        Prev(&it); // skip the end token we were on
        while (IsValid(&it))
        {
            Token *t = Prev(&it);
            if (!t) break;

            if (IsInNest(&nests, t->kind, Direction_Backward))
            {
                continue;
            }

            if (t->kind == Token_LeftParen ||
                t->kind == ',')
            {
                int64_t inner_start_pos = t->pos + t->length;

                Token *next = PeekNext(&it, 1);
                if (next)
                {
                    inner_start_pos = next->pos;
                }

                int64_t start_pos = inner_start_pos;
                if (t->kind  == ',' &&
                    end_kind == Token_RightParen)
                {
                    start_pos = t->pos;
                }

                result.selection.inner.start = inner_start_pos;
                result.selection.inner.end   = inner_end_pos;
                result.selection.outer.start = start_pos;
                result.selection.outer.end   = end_pos;

                result.pos = result.selection.outer.end;

                break;
            }
        }
    }

    return result;
}

MOVEMENT_PROC(MoveDown)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t line = GetLineNumber(buffer, cursor->pos);

    Move move = {};
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, 1)).pos;
    GetLineRanges(buffer, line, &move.selection.inner, &move.selection.outer);
    return move;
}

MOVEMENT_PROC(MoveUp)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t line = GetLineNumber(buffer, cursor->pos);

    Move move = {};
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, -1)).pos;
    GetLineRanges(buffer, line, &move.selection.inner, &move.selection.outer);
    return move;
}

COMMAND_PROC(PageUp)
{
    View *view = GetActiveView();
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, -Max(0, viewport_height - 4)));
}

COMMAND_PROC(PageDown)
{
    View *view = GetActiveView();
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, Max(0, viewport_height - 4)));
}

COMMAND_PROC(BackspaceChar)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    int64_t newline_length = PeekNewlineBackward(buffer, pos - 1);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }

    int64_t line_start = FindLineStart(buffer, pos);

    int64_t consecutive_space_count = 0;
    for (int64_t at = pos - 1; at > line_start; at -= 1)
    {
        if (ReadBufferByte(buffer, at) == ' ')
        {
            consecutive_space_count += 1;
        }
        else
        {
            break;
        }
    }
    if ((consecutive_space_count > 0) &&
        ((consecutive_space_count % core_config->indent_width) == 0))
    {
        to_delete = core_config->indent_width;
    }

    pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos - to_delete, to_delete), ""_str);
    SetCursor(view, pos);
}

COMMAND_PROC(BackspaceWord)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    int64_t start_pos = pos;
    int64_t end_pos = ScanWordBackward(buffer, pos).end;
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursor(view, final_pos);
}

COMMAND_PROC(DeleteChar)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    int64_t newline_length = PeekNewline(buffer, pos);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos, to_delete), ""_str);
    SetCursor(view, pos);
}

COMMAND_PROC(DeleteWord)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;

    int64_t start_pos = pos;
    int64_t end_pos = ScanWordEndForward(buffer, pos);
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursor(view, final_pos);
}

COMMAND_PROC(UndoOnce)
{
    View *view = GetActiveView();

    Range result = UndoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result);
    }
}

COMMAND_PROC(RedoOnce)
{
    View *view = GetActiveView();

    Range result = RedoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result);
    }
}

COMMAND_PROC(SelectNextUndoBranch)
{
    SelectNextUndoBranch(GetActiveBuffer());
}

COMMAND_PROC(SplitWindowVertical, "Split the window along the vertical axis"_str)
{
    Window *window = editor->active_window;
    SplitWindow(window, WindowSplit_Vert);
}

COMMAND_PROC(SplitWindowHorizontal, "Split the window along the horizontal axis"_str)
{
    Window *window = editor->active_window;
    SplitWindow(window, WindowSplit_Horz);
}

COMMAND_PROC(DestroyWindow, "Destroy the current active window"_str)
{
    Window *window = editor->active_window;
    DestroyWindow(window);
}

COMMAND_PROC(FocusWindowLeft, "Focus the next window on the left"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Vert) &&
             (window->parent->first_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->prev;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->last_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowRight, "Focus the next window on the right"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Vert) &&
             (window->parent->last_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->next;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->first_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowDown, "Focus the next window down"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Horz) &&
             (window->parent->last_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->next;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->first_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowUp, "Focus the next window above"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Horz) &&
             (window->parent->first_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->prev;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->last_child;
        }
        editor->active_window = window;
    }
}

CHANGE_PROC(DeleteSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    BufferReplaceRange(buffer, selection.outer, ""_str);
}

CHANGE_PROC(DeleteInnerSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    BufferReplaceRange(buffer, selection.inner, ""_str);
}

CHANGE_PROC(ChangeSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = selection.inner.start;

    Range line_range = GetLineRange(buffer, selection.inner);

    BeginUndoBatch(GetActiveBuffer());
    BufferReplaceRange(buffer, selection.inner, ""_str);
    if (line_range.start != line_range.end)
    {
        AutoIndentLineAt(buffer, cursor->pos);
    }
    CMD_EnterTextMode();
}

CHANGE_PROC(ChangeOuterSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = selection.outer.start;

    Range line_range = GetLineRange(buffer, selection.outer);

    BeginUndoBatch(GetActiveBuffer());
    BufferReplaceRange(buffer, selection.outer, ""_str);
    if (line_range.start != line_range.end)
    {
        AutoIndentLineAt(buffer, cursor->pos);
    }
    CMD_EnterTextMode();
}

CHANGE_PROC(ToUppercase)
{
    Buffer *buffer = GetActiveBuffer();
    String string = BufferPushRange(platform->GetTempArena(), buffer, selection.inner);
    for (size_t i = 0; i < string.size; i += 1)
    {
        string.data[i] = ToUpperAscii(string.data[i]);
    }
    BufferReplaceRange(buffer, selection.inner, string);
}

COMMAND_PROC(RepeatLastCommand)
{
    // dummy command
}

COMMAND_PROC(Copy)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Range range = SanitizeRange(cursor->selection.outer);
    String string = BufferSubstring(buffer, range);

    platform->WriteClipboard(string);
}

COMMAND_PROC(PasteBefore)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

COMMAND_PROC(PasteAfter)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos + 1;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

CHANGE_PROC(PasteReplaceSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp_memory(arena);

    String replaced_string = BufferPushRange(arena, buffer, selection.outer);
    String string = platform->ReadClipboard(arena);

    int64_t pos = BufferReplaceRange(buffer, selection.outer, string);
    SetCursor(view, pos);

    platform->WriteClipboard(replaced_string);
}

COMMAND_PROC(OpenNewLineBelow)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    CMD_EnterTextMode();

    String line_end = LineEndString(buffer->line_end);

    int64_t insert_pos = FindLineEnd(buffer, cursor->pos).outer;
    BufferReplaceRange(buffer, MakeRange(insert_pos), line_end);
    int64_t new_pos = AutoIndentLineAt(buffer, insert_pos);
    SetCursor(view, new_pos);
}

TEXT_COMMAND_PROC(WriteText)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    uint8_t     buf[256];
    int         buf_at           = 0;

    size_t      size             = text.size;
    uint8_t    *data             = text.data;
    int         indent_width     = core_config->indent_width;
    bool        indent_with_tabs = core_config->indent_with_tabs;
    LineEndKind line_end_kind    = buffer->line_end;

    bool should_auto_indent = false;
    for (size_t i = 0; i < size; i += 1)
    {
        uint8_t c = data[i];
        
        if (c == '\n' ||
            c == '}'  ||
            c == ')')
        {
            should_auto_indent = true;
        }

        if (!indent_with_tabs && c == '\t')
        {
            int left = sizeof(buf) - buf_at;

            int spaces = indent_width;
            if (spaces > left) spaces = left;

            for (int j = 0; j < spaces; j += 1)
            {
                buf[buf_at++] = ' ';
            }
        }
        else if ((line_end_kind == LineEnd_CRLF) && (c == '\n'))
        {
            buf[buf_at++] = '\r';
            buf[buf_at++] = '\n';
        }
        else
        {
            if (buf_at < (int)sizeof(buf) &&
                (c == '\n' ||
                 c == '\t' ||
                 (c >= ' ' && c <= '~') ||
                 (c >= 128)))
            {
                buf[buf_at++] = c;
            }
        }
    }

    if (buf_at > 0)
    {
        Cursor *cursor = GetCursor(view);
        int64_t pos = cursor->pos;
        int64_t new_pos = BufferReplaceRange(buffer, MakeRange(pos), MakeString(buf_at, buf));
        if (should_auto_indent)
        {
            AutoIndentLineAt(buffer, new_pos);
        }
    }
}

function void
OnMouseDown(void)
{
    View *view = GetActiveView();
    Cursor *cursor = GetCursor(view);

    if (editor->token_at_mouse)
    {
        Token *t = editor->token_at_mouse;
        if (cursor->selection.inner.start == t->pos &&
            cursor->selection.inner.end   == t->pos + t->length)
        {
            if (cursor->pos == cursor->selection.inner.start)
            {
                cursor->pos = cursor->selection.inner.end;
            }
            else if (cursor->pos == cursor->selection.inner.end)
            {
                cursor->pos = cursor->selection.inner.start;
            }
        }
        else
        {
            cursor->pos = t->pos;
            cursor->selection = MakeSelection(MakeRangeStartLength(t->pos, t->length));
        }
    }
    else
    {
        cursor->pos = editor->pos_at_mouse;
        cursor->selection = MakeSelection(MakeRange(cursor->pos));
    }
}

function void
OnMouseHeld(void)
{
}

function void
OnMouseUp(void)
{
}
