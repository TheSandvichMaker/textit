COMMAND_PROC(ForceTick,
             "Force the editor to tick"_str)
{
    UNUSED_VARIABLE(editor);
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
    editor->input_mode = InputMode_CommandLine;
}

COMMAND_PROC(Exit, "Exit the editor"_str)
{
    UNUSED_VARIABLE(editor);
    platform->exit_requested = true;
}

COMMAND_PROC(qa, "Exit the editor"_str)
{
    UNUSED_VARIABLE(editor);
    platform->exit_requested = true;
}

COMMAND_PROC(ToggleVisualizeNewlines, "Toggle the visualization of newlines"_str)
{
    UNUSED_VARIABLE(editor);
    core_config->visualize_newlines = !core_config->visualize_newlines;
}

COMMAND_PROC(ToggleVisualizeWhitespace, "Toggle the visualization of whitespaces"_str)
{
    UNUSED_VARIABLE(editor);
    core_config->visualize_whitespace = !core_config->visualize_whitespace;
}

COMMAND_PROC(EnterTextMode,
             "Enter Text Input Mode"_str)
{
    editor->next_edit_mode = EditMode_Text;
    BeginUndoBatch(CurrentBuffer(editor));
}

COMMAND_PROC(Append)
{
    View *view = CurrentView(editor);
    Cursor *cursor = GetCursor(view);
    cursor->pos += 1;
    CMD_EnterTextMode(editor);
}

COMMAND_PROC(AppendAtEnd)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);
    cursor->pos = FindLineEnd(buffer, cursor->pos) + 1;
    CMD_EnterTextMode(editor);
}

COMMAND_PROC(EnterCommandMode,
             "Enter Command Mode"_str)
{
    editor->next_edit_mode = EditMode_Command;

    Buffer *buffer = CurrentBuffer(editor);
    EndUndoBatch(buffer);

    View *view = CurrentView(editor);
    Cursor *cursor = GetCursor(view);

    BufferLocation loc = CalculateRelativeMove(buffer, cursor, MakeV2i(-1, 0));
    cursor->pos = loc.pos;
    cursor->selection = MakeRange(cursor->pos);
    cursor->sticky_col = loc.col;
}

COMMAND_PROC(CenterView,
             "Center the view around the cursor"_str)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    view->scroll_at = loc.line - viewport_height / 2;
}

COMMAND_PROC(JumpToBufferStart, "Jump to the start of the buffer"_str)
{
    View *view = CurrentView(editor);
    SetCursor(view, 0);
}

COMMAND_PROC(JumpToBufferEnd, "Jump to the end of the buffer"_str)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    SetCursor(view, buffer->count - 1);
}

MOVEMENT_PROC(MoveLeft)
{
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range result = ScanWordBackward(buffer, pos);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.end);
    cursor->sticky_col = loc.col;

    return MakeMove(result);
}

MOVEMENT_PROC(MoveRightIdentifier)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range result = ScanWordForward(buffer, pos);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.end);
    cursor->sticky_col = loc.col;

    return MakeMove(result);
}

MOVEMENT_PROC(MoveLineStart)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 0;

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(MakeRange(pos, line_range.start));
}

MOVEMENT_PROC(MoveLineEnd)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 9999;

    int64_t pos = cursor->pos;
    int64_t end = FindLineEnd(buffer, pos);

    Move move = {};
    move.selection.start = pos;
    move.selection.end   = end;
    move.pos             = end;

    return move;
}

MOVEMENT_PROC(EncloseLine)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(line_range);
}

MOVEMENT_PROC(EncloseNextScope)
{
    View *view = CurrentView(editor);
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
                    result.end = (seek_forward ? (t->pos + t->length - 1) : t->pos);
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
                      bool line_selection,
                      bool inner_selection)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move result = {};
    result.flags     = MoveFlag_NoAutoRepeat;
    result.pos       = cursor->pos;
    result.selection = cursor->selection;
    
    int64_t pos = cursor->pos;

    int64_t start_pos = -1;

    TokenIterator it = SeekTokenIterator(buffer, pos);
    if (inner_selection)
    {
        Prev(&it);
    }

    int depth = 0;
    while (IsValid(&it))
    {
        Token *t = Prev(&it);
        if (t->kind == close_nest)
        {
            depth += 1;
        }
        if (t->kind == open_nest)
        {
            depth -= 1;
            if (depth < 0)
            {
                start_pos = (inner_selection ? t->pos + t->length : t->pos);
                break;
            }
        }
    }

    while (IsValid(&it))
    {
        Token *t = Next(&it);
        if (t->kind == open_nest)
        {
            depth += 1;
        }
        else if (t->kind == close_nest)
        {
            depth -= 1;
            if (depth < 0)
            {
                result.pos             = start_pos;
                result.selection.start = start_pos;
                result.selection.end   = (inner_selection ? t->pos : t->pos + t->length);
                break;
            }
        }
    }

    if (line_selection)
    {
        int64_t start_line = GetLineNumber(buffer, result.selection.start);
        int64_t end_line   = GetLineNumber(buffer, result.selection.end);
        if (inner_selection)
        {
            start_line += 1;
            end_line    = Max(start_line, end_line - 1);
        }
        if (start_line != end_line)
        {
            result.selection.start = GetInnerLineRange(buffer, start_line).start;
            result.selection.end   = GetInnerLineRange(buffer, end_line).end;
        }
        result.pos = result.selection.start;
    }

    result.selection.end -= 1; // PLEASE FOR HTE LOVE OF GOD STOP THIS INCLUSIVE RANGE GIBBERISH OH MY GOD WHY WHY WHY etc (dramatic whining)

    return result;
}

MOVEMENT_PROC(EncloseSurroundingScope)
{
    View *view = CurrentView(editor);
    return SelectSurroundingNest(view, Token_LeftScope, Token_RightScope, true, true);
}

MOVEMENT_PROC(EncloseSurroundingParen)
{
    View *view = CurrentView(editor);
    return SelectSurroundingNest(view, Token_LeftParen, Token_RightParen, false, true);
}

MOVEMENT_PROC(MoveDown)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move move = {};
    move.selection = TrimEnd(EncloseLine(buffer, cursor->pos, true), 1);
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, 1)).pos;
    return move;
}

MOVEMENT_PROC(MoveUp)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move move = {};
    move.selection = TrimEnd(EncloseLine(buffer, cursor->pos, true), 1);
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, -1)).pos;
    return move;
}

COMMAND_PROC(PageUp)
{
    View *view = CurrentView(editor);
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, -Max(0, viewport_height - 4)));
}

COMMAND_PROC(PageDown)
{
    View *view = CurrentView(editor);
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, Max(0, viewport_height - 4)));
}

COMMAND_PROC(BackspaceChar)
{
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);
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
    View *view = CurrentView(editor);

    Range result = UndoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result.end);
    }
}

COMMAND_PROC(RedoOnce)
{
    View *view = CurrentView(editor);

    Range result = RedoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result.end);
    }
}

COMMAND_PROC(SelectNextUndoBranch)
{
    SelectNextUndoBranch(CurrentBuffer(editor));
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
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    BufferReplaceRange(buffer, range, ""_str);
}

CHANGE_PROC(ChangeSelection)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    BeginUndoBatch(CurrentBuffer(editor));
    CHG_DeleteSelection(editor, range);
    AutoIndentLineAt(buffer, cursor->pos);
    CMD_EnterTextMode(editor);
}

CHANGE_PROC(ToUppercase)
{
    Buffer *buffer = CurrentBuffer(editor);
    String string = BufferPushRange(platform->GetTempArena(), buffer, range);
    for (size_t i = 0; i < string.size; i += 1)
    {
        string.data[i] = ToUpperAscii(string.data[i]);
    }
    BufferReplaceRange(buffer, range, string);
}

COMMAND_PROC(RepeatLastCommand)
{
    UNUSED_VARIABLE(editor);
    // dummy command
}

COMMAND_PROC(Copy)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Range range = GetEditRange(cursor);
    String string = BufferSubstring(buffer, range);

    platform->WriteClipboard(string);
}

COMMAND_PROC(PasteBefore)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

COMMAND_PROC(PasteAfter)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos + 1;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

CHANGE_PROC(PasteReplaceSelection)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp_memory(arena);

    String replaced_string = BufferPushRange(arena, buffer, range);
    String string = platform->ReadClipboard(arena);

    int64_t pos = BufferReplaceRange(buffer, range, string);
    SetCursor(view, pos);

    platform->WriteClipboard(replaced_string);
}

COMMAND_PROC(OpenNewLineBelow)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    CMD_EnterTextMode(editor);

    String line_end = LineEndString(buffer->line_end);

    int64_t insert_pos = FindLineEnd(buffer, cursor->pos, true);
    BufferReplaceRange(buffer, MakeRange(insert_pos), line_end);
    int64_t new_pos = AutoIndentLineAt(buffer, insert_pos);
    SetCursor(view, new_pos);
}

TEXT_COMMAND_PROC(WriteText)
{
    View   *view   = CurrentView(editor);
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
    View *view = CurrentView(editor_state);
    Cursor *cursor = GetCursor(view);

    if (editor_state->token_at_mouse)
    {
        Token *t = editor_state->token_at_mouse;
        if (cursor->selection.start == t->pos &&
            cursor->selection.end   == t->pos + t->length - 1)
        {
            if (cursor->pos == cursor->selection.start)
            {
                cursor->pos = cursor->selection.end;
            }
            else if (cursor->pos == cursor->selection.end)
            {
                cursor->pos = cursor->selection.start;
            }
        }
        else
        {
            cursor->pos = t->pos;
            cursor->selection = MakeRangeStartLength(t->pos, t->length - 1);
        }
    }
    else
    {
        cursor->pos = editor_state->pos_at_mouse;
        cursor->selection = MakeRange(cursor->pos);
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
