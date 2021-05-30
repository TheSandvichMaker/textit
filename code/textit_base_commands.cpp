COMMAND_PROC(EnterTextMode,
             "Enter Text Input Mode"_str)
{
    editor->next_edit_mode = EditMode_Text;
    editor->enter_text_mode_undo_ordinal = CurrentUndoOrdinal(CurrentBuffer(editor));
}

COMMAND_PROC(EnterCommandMode,
             "Enter Command Mode"_str)
{
    editor->next_edit_mode = EditMode_Command;

    Buffer *buffer = CurrentBuffer(editor);
    uint64_t current_undo_ordinal = CurrentUndoOrdinal(buffer);
    MergeUndoHistory(buffer, editor->enter_text_mode_undo_ordinal, current_undo_ordinal);
}

MOVEMENT_PROC(MoveLeft)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos - 1, line_range);
    return MakeRange(pos);
}

MOVEMENT_PROC(MoveRight)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos + 1, line_range);
    return MakeRange(pos);
}

MOVEMENT_PROC(EncloseLine)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    Range line_range = EncloseLine(buffer, pos);
    return line_range;
}

COMMAND_PROC(MoveDown)
{
    View *view = CurrentView(editor);
    MoveCursorRelative(view, MakeV2i(0, 1));
}

COMMAND_PROC(MoveUp)
{
    View *view = CurrentView(editor);
    MoveCursorRelative(view, MakeV2i(0, -1));
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

MOVEMENT_PROC(MoveLeftIdentifier)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    Range line_range = EncloseLine(buffer, pos);

    Range result = ScanWordBackward(buffer, pos);
    result = ClampRange(result, line_range);
    return result;
}

MOVEMENT_PROC(MoveRightIdentifier)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    Range line_range = EncloseLine(buffer, pos);

    Range result = ScanWordForward(buffer, pos);
    result = ClampRange(result, line_range);
    return result;
}

COMMAND_PROC(BackspaceChar)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    int64_t newline_length = PeekNewlineBackward(buffer, pos - 1);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos - to_delete, to_delete), ""_str);
    SetCursorPos(view, pos);
}

COMMAND_PROC(BackspaceWord)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    int64_t start_pos = pos;
    int64_t end_pos = ScanWordBackward(buffer, pos).end;
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursorPos(view, final_pos);
}

COMMAND_PROC(DeleteChar)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);
    int64_t newline_length = PeekNewline(buffer, pos);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos, to_delete), ""_str);
    SetCursorPos(view, pos);
}

COMMAND_PROC(DeleteWord)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);

    int64_t start_pos = pos;
    int64_t end_pos = ScanWordEndForward(buffer, pos);
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursorPos(view, final_pos);
}

COMMAND_PROC(UndoOnce)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = UndoOnce(buffer);
    if (pos >= 0)
    {
        SetCursorPos(view, pos);
    }
}

COMMAND_PROC(RedoOnce)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = RedoOnce(buffer);
    if (pos >= 0)
    {
        SetCursorPos(view, pos);
    }
}

COMMAND_PROC(SelectNextUndoBranch)
{
    SelectNextUndoBranch(CurrentBuffer(editor));
}

COMMAND_PROC(SplitWindowVertical)
{
    Window *window = &editor->root_window;
    SplitWindow(window, WindowSplit_Vert);
}

CHANGE_PROC(DeleteSelection)
{
    Buffer *buffer = CurrentBuffer(editor);
    // Range mark_range = MakeSanitaryRange(buffer->cursor.pos, buffer->mark.pos);
    // mark_range.end += 1;
    int64_t pos = BufferReplaceRange(buffer, range, ""_str);
    SetCursor(buffer, pos);
}

CHANGE_PROC(ChangeSelection)
{
    CHG_DeleteSelection(editor, range);
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

TEXT_COMMAND_PROC(WriteText)
{
    View *view = CurrentView(editor);
    Buffer *buffer = GetBuffer(view);

    int64_t pos = GetCursor(buffer);

    uint64_t start_ordinal = CurrentUndoOrdinal(buffer);
    bool should_merge = false;
    for (size_t i = 0; i < text.size; i += 1)
    {
        if (text.data[0] != '\n')
        {
            UndoNode *last_undo = CurrentUndoNode(buffer);

            String last_text = last_undo->forward;
            if ((last_undo->pos == pos - 1) &&
                (last_text.size > 0))
            {
                if ((text.data[0] == ' ') &&
                    ((last_text.data[0] == ' ') ||
                     IsValidIdentifierAscii(last_text.data[0])))
                {
                    should_merge = true;
                }
                else if (IsValidIdentifierAscii(text.data[0]) &&
                         IsValidIdentifierAscii(last_text.data[0]))
                {
                    should_merge = true;
                }
            }
        }
    }
    if (text.data[0] == '\n' || IsPrintableAscii(text.data[0]) || IsUtf8Byte(text.data[0]))
    {
        int64_t new_pos = BufferReplaceRange(buffer, MakeRange(pos), text);
        SetCursorPos(view, new_pos);
    }
    if (should_merge)
    {
        uint64_t end_ordinal = CurrentUndoOrdinal(buffer);
        MergeUndoHistory(buffer, start_ordinal, end_ordinal);
    }
}
