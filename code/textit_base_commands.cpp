static inline void
CMD_EnterTextMode(EditorState *editor)
{
    editor->next_edit_mode = EditMode_Text;
    editor->enter_text_mode_undo_ordinal = CurrentUndoOrdinal(CurrentBuffer(editor));
}
REGISTER_COMMAND(CMD_EnterTextMode);

static inline void
CMD_EnterCommandMode(EditorState *editor)
{
    editor->next_edit_mode = EditMode_Command;

    Buffer *buffer = CurrentBuffer(editor);
    uint64_t current_undo_ordinal = CurrentUndoOrdinal(buffer);
    MergeUndoHistory(buffer, editor->enter_text_mode_undo_ordinal, current_undo_ordinal);
}
REGISTER_COMMAND(CMD_EnterCommandMode);

static inline void
CMD_MoveLeft(EditorState *editor)
{
    View *view = editor->open_view;
    MoveCursorRelative(view, MakeV2i(-1, 0));
}
REGISTER_COMMAND(CMD_MoveLeft);

static inline void
CMD_MoveRight(EditorState *editor)
{
    View *view = editor->open_view;
    MoveCursorRelative(view, MakeV2i(1, 0));
}
REGISTER_COMMAND(CMD_MoveRight);

static inline void
CMD_MoveDown(EditorState *editor)
{
    View *view = editor->open_view;
    MoveCursorRelative(view, MakeV2i(0, 1));
}
REGISTER_COMMAND(CMD_MoveDown);

static inline void
CMD_MoveUp(EditorState *editor)
{
    View *view = editor->open_view;
    MoveCursorRelative(view, MakeV2i(0, -1));
}
REGISTER_COMMAND(CMD_MoveUp);

static inline void
CMD_MoveLeftIdentifier(EditorState *editor)
{
    View *view = editor->open_view;
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);
    int64_t pos = ScanWordBackward(buffer, loc.pos);
    pos = ClampToRange(pos, loc.line_range);
    SetCursorPos(view, pos);
}
REGISTER_COMMAND(CMD_MoveLeftIdentifier);

static inline void
CMD_MoveRightIdentifier(EditorState *editor)
{
    View *view = editor->open_view;
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);
    int64_t pos = ScanWordForward(buffer, loc.pos);
    pos = ClampToRange(pos, loc.line_range);
    SetCursorPos(view, pos);
}
REGISTER_COMMAND(CMD_MoveRightIdentifier);

static inline void
CMD_BackspaceChar(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t newline_length = PeekNewlineBackward(buffer, loc.pos - 1);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    int64_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos - to_delete, to_delete), ""_str);
    SetCursorPos(view, pos);
}
REGISTER_COMMAND(CMD_BackspaceChar);

static inline void
CMD_BackspaceWord(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t start_pos = loc.pos;
    int64_t end_pos = ScanWordBackward(buffer, loc.pos);
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursorPos(view, final_pos);
}
REGISTER_COMMAND(CMD_BackspaceWord);

static inline void
CMD_DeleteChar(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t newline_length = PeekNewline(buffer, loc.pos);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    int64_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos, to_delete), ""_str);
    SetCursorPos(view, pos);
}
REGISTER_COMMAND(CMD_DeleteChar);

static inline void
CMD_DeleteWord(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t start_pos = loc.pos;
    int64_t end_pos = ScanWordEndForward(buffer, loc.pos);
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursorPos(view, final_pos);
}
REGISTER_COMMAND(CMD_DeleteWord);

static inline void
CMD_UndoOnce(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    int64_t pos = UndoOnce(buffer);
    if (pos >= 0)
    {
        SetCursorPos(view, pos);
    }
}
REGISTER_COMMAND(CMD_UndoOnce);

static inline void
CMD_RedoOnce(EditorState *editor)
{
    View *view = CurrentView(editor);
    Buffer *buffer = view->buffer;

    int64_t pos = RedoOnce(buffer);
    if (pos >= 0)
    {
        SetCursorPos(view, pos);
    }
}
REGISTER_COMMAND(CMD_RedoOnce);

static inline void
CMD_SelectNextUndoBranch(EditorState *editor)
{
    SelectNextUndoBranch(CurrentBuffer(editor));
}
REGISTER_COMMAND(CMD_SelectNextUndoBranch);

static inline void
CMD_WriteText(EditorState *editor, String text)
{
    View *view = editor->open_view;
    Buffer *buffer = view->buffer;
    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    uint64_t start_ordinal = CurrentUndoOrdinal(buffer);
    bool should_merge = false;
    for (size_t i = 0; i < text.size; i += 1)
    {
        if (text.data[0] != '\n')
        {
            UndoNode *last_undo = CurrentUndoNode(buffer);

            String last_text = last_undo->forward;
            if ((last_undo->pos == loc.pos - 1) &&
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
        int64_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), text);
        SetCursorPos(view, pos);
    }
    if (should_merge)
    {
        uint64_t end_ordinal = CurrentUndoOrdinal(buffer);
        MergeUndoHistory(buffer, start_ordinal, end_ordinal);
    }
}
REGISTER_COMMAND(CMD_WriteText);

