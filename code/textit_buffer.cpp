function LineEndKind
GuessLineEndKind(String string)
{
    int64_t lf   = 0;
    int64_t crlf = 0;
    for (size_t i = 0; i < string.size; i += 1)
    {
        if (string.data[i + 0] == '\r' &&
            string.data[i + 1] == '\n')
        {
            crlf += 1;
            i += 1;
        }
        else if (string.data[i] == '\n')
        {
            lf += 1;
        }
    }

    LineEndKind result = LineEnd_LF;
    if (crlf > lf)
    {
        result = LineEnd_CRLF;
    }
    return result;
}

function uint8_t
ReadBufferByte(Buffer *buffer, int64_t pos)
{
    uint8_t result = 0;
    if (IsInBufferRange(buffer, pos))
    {
        result = buffer->text[pos];
    }
    return result;
}

function int64_t
PeekNewline(Buffer *buffer, int64_t pos)
{
    if (ReadBufferByte(buffer, pos + 0) == '\r' &&
        ReadBufferByte(buffer, pos + 1) == '\n')
    {
        return 2;
    }
    if (ReadBufferByte(buffer, pos) == '\n')
    {
        return 1;
    }
    return 0;
}

function bool
AdvanceOverNewline(Buffer *buffer, int64_t *pos)
{
    int64_t length = PeekNewline(buffer, *pos);

    bool result = (length > 0);
    if (result)
    {
        *pos = *pos + length;
    }
    else
    {
        *pos = *pos + 1;
    }
    return result;
}

function int64_t
PeekNewlineBackward(Buffer *buffer, int64_t pos)
{
    if (ReadBufferByte(buffer, pos - 0) == '\n' &&
        ReadBufferByte(buffer, pos - 1) == '\r')
    {
        return 2;
    }
    if (ReadBufferByte(buffer, pos) == '\n')
    {
        return 1;
    }
    return 0;
}

function int64_t
ScanWordEndForward(Buffer *buffer, int64_t pos)
{
    bool skipped_whitespace = false;
    if (IsInBufferRange(buffer, pos) &&
        IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        skipped_whitespace = true;
        while (IsInBufferRange(buffer, pos) &&
               IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
            if (ReadBufferByte(buffer, pos) == '\n')
            {
                return pos;
            }
        }
    }
    if (IsInBufferRange(buffer, pos) &&
        IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
    {
        while (IsInBufferRange(buffer, pos) && IsAlphanumericAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    else if (!skipped_whitespace)
    {
        while (IsInBufferRange(buffer, pos) &&
               !IsAlphanumericAscii(ReadBufferByte(buffer, pos)) && !IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
        {
            pos += 1;
        }
    }
    return pos;
}

function Selection
ScanWordForward2(Buffer *buffer, int64_t pos)
{
    Selection result = {};

    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos += 1;
    }

    result.inner = MakeRange(pos);
    result.outer = MakeRange(pos);

    CharacterClassFlags match_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    while (IsInBufferRange(buffer, pos) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos)) == match_class)
    {
        pos += 1;
    }
    result.inner.end = pos;

    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos += 1;
    }
    result.outer.end = pos;

    return result;
}
function Selection
ScanWordBackward2(Buffer *buffer, int64_t pos)
{
    Selection result = {};
    result.inner = MakeRange(pos);
    result.outer = MakeRange(pos);

    if (IsInBufferRange(buffer, pos - 1))
    {
        pos -= 1;
    }

    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos -= 1;
    }
    result.inner.start = pos + 1;

    CharacterClassFlags match_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    while (IsInBufferRange(buffer, pos - 1) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos - 1)) == match_class)
    {
        pos -= 1;
    }
    result.inner.end = pos;
    result.outer.end = pos;

    return result;
}

function Range
ScanWordForward(Buffer *buffer, int64_t pos)
{
    if (PeekNewline(buffer, pos + 1))
    {
        return MakeRange(pos + 1 + PeekNewline(buffer, pos + 1));
    }

    CharacterClassFlags p0_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    CharacterClassFlags p1_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos + 1));
    if (p0_class != p1_class)
    {
        pos += 1;
    }

    Range result = MakeRange(pos);

    while (IsInBufferRange(buffer, pos + 1) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos + 1)) == p1_class)
    {
        pos += 1;
    }

    while (IsInBufferRange(buffer, pos + 1) &&
           IsHorizontalWhitespaceAscii(ReadBufferByte(buffer, pos + 1)))
    {
        pos += 1;
    }

    result.end = pos;
    return result;
}

function Range
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    if (PeekNewlineBackward(buffer, pos - 1))
    {
        pos -= PeekNewlineBackward(buffer, pos - 1);
        if (!PeekNewlineBackward(buffer, pos - 1))
        {
            pos -= 1;
        }
        return MakeRange(pos);
    }

    CharacterClassFlags p0_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));
    CharacterClassFlags p1_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos - 1));
    if (!HasFlag(p0_class, Character_Whitespace) && (p0_class != p1_class))
    {
        pos -= 1;
    }

    Range result = MakeRange(pos);

    while (IsInBufferRange(buffer, pos) &&
           IsHorizontalWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        pos -= 1;
    }

    CharacterClassFlags skip_class = CharacterizeByteLoosely(ReadBufferByte(buffer, pos));

    while (IsInBufferRange(buffer, pos - 1) &&
           CharacterizeByteLoosely(ReadBufferByte(buffer, pos - 1)) == skip_class)
    {
        pos -= 1;
    }

    while (IsInBufferRange(buffer, pos) &&
           IsVerticalWhitespaceAscii(ReadBufferByte(buffer, pos)))
       {
           pos -= 1;
       }

    result.end = pos;
    return result;
}

function int64_t
FindLineStart(Buffer *buffer, int64_t pos)
{
    int64_t result = pos;

    while (IsInBufferRange(buffer, result - 1) && !PeekNewlineBackward(buffer, result - 1))
    {
        result -= 1;
    }

    return result;
}

function struct { int64_t inner; int64_t outer; }
FindLineEnd(Buffer *buffer, int64_t pos)
{
    int64_t inner = pos;
    int64_t outer = pos;

    while (IsInBufferRange(buffer, inner))
    {
        int64_t newline_length = PeekNewline(buffer, inner);
        if (newline_length)
        {
            outer = inner + newline_length;
            break;
        }
        else
        {
            inner += 1;
        }
    }

    return { inner, outer };
}

function int64_t
FindFirstNonHorzWhitespace(Buffer *buffer, int64_t pos)
{
    int64_t result = pos;
    while (IsInBufferRange(buffer, result) && IsHorizontalWhitespaceAscii(buffer->text[result]))
    {
        result += 1;
    }
    return result;
}

function Range
EncloseLine(Buffer *buffer, int64_t pos, bool including_newline = false)
{
    Range result = MakeRange(pos, pos);
    while (IsInBufferRange(buffer, result.start - 1) && !PeekNewlineBackward(buffer, result.start - 1))
    {
        result.start -= 1;
    }

    int64_t newline_length = 0;
    while (IsInBufferRange(buffer, result.end + 1))
    {
        newline_length = PeekNewline(buffer, result.end + 1);
        result.end += 1;
        if (newline_length)
        {
            if (including_newline)
            {
                result.end += newline_length;
            }
            break;
        }
    }
    return result;
}

function Range
GetLineRange(Buffer *buffer, int64_t line)
{
    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);
    return info.range;
}

function Range
GetInnerLineRange(Buffer *buffer, int64_t line)
{
    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    Range result = MakeRange(info.range.start, info.newline_pos);
    return result;
}

function void
GetLineRanges(Buffer *buffer, int64_t line, Range *inner, Range *outer)
{
    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    *outer = info.range;

    *inner = info.range;
    inner->start = FindFirstNonHorzWhitespace(buffer, info.range.start);
    inner->end   = info.newline_pos;
}

function BufferLocation
CalculateBufferLocationFromPos(Buffer *buffer, int64_t pos)
{
    pos = ClampToBufferRange(buffer, pos);

    BufferLocation result = {};

    LineInfo info;
    FindLineInfoByPos(buffer, pos, &info);

    result.line       = info.line;
    result.pos        = pos;
    result.col        = pos - info.range.start;
    result.line_range = info.range;

    return result;
}

function int64_t
GetLineNumber(Buffer *buffer, int64_t pos)
{
    return CalculateBufferLocationFromPos(buffer, pos).line;
}

function Range
GetLineRange(Buffer *buffer, Range range)
{
    range = ClampRange(range, BufferRange(buffer));

    Range result = {};
    result.start = CalculateBufferLocationFromPos(buffer, range.start).line;
    result.end   = CalculateBufferLocationFromPos(buffer, range.end).line;

    return result;
}

function BufferLocation
CalculateBufferLocationFromLineCol(Buffer *buffer, int64_t line, int64_t col)
{
    BufferLocation result = {};

    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    result.line       = info.line;
    result.col        = Clamp(col, 0, info.newline_pos - info.range.start);
    result.pos        = info.range.start + result.col;
    result.line_range = info.range;

    return result;
}

function BufferLocation
CalculateRelativeMove(Buffer *buffer, Cursor *cursor, V2i delta)
{
    BufferLocation curr_loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    int64_t target_line = curr_loc.line + delta.y;
    int64_t target_col  = curr_loc.col  + delta.x;

    if (!delta.x && delta.y)
    {
        target_col = cursor->sticky_col;
    }

    BufferLocation result = CalculateBufferLocationFromLineCol(buffer, target_line, target_col);

    if (delta.x)
    {
        cursor->sticky_col = result.col;
    }

    return result;
}

function void
PushUndoInternal(Buffer *buffer, int64_t pos, String forward, String backward)
{
    auto undo = &buffer->undo;

    UndoNode *node = PushStruct(&buffer->arena, UndoNode);

    node->parent = undo->at;
    node->next_child = node->parent->first_child;
    node->parent->first_child = node;

    undo->current_ordinal += 1;
    node->ordinal = undo->current_ordinal;

    node->pos = pos;
    node->forward = forward;
    node->backward = backward;

    undo->at = node;
    undo->depth += 1;
}

function void
FlushBufferedUndo(Buffer *buffer)
{
    UNUSED_VARIABLE(buffer);
    /* TODO: Reimplement */
}

function void
PushUndo(Buffer *buffer, int64_t pos, String forward, String backward)
{
    FlushBufferedUndo(buffer);
    PushUndoInternal(buffer, pos, forward, backward);
}

function UndoNode *
CurrentUndoNode(Buffer *buffer)
{
    auto undo = &buffer->undo;
    return undo->at;
}

function void
SelectNextUndoBranch(Buffer *buffer)
{
    UndoNode *node = CurrentUndoNode(buffer);

    uint32_t child_count = 0;
    for (UndoNode *child = node->first_child;
         child;
         child = child->next_child)
    {
        child_count += 1;
    }

    node->selected_branch = (node->selected_branch + 1) % child_count;
}

function UndoNode *
NextChild(UndoNode *node)
{
    UndoNode *result = node->first_child;
    for (uint32_t i = 0; i < node->selected_branch; i += 1)
    {
        result = result->next_child;
    }
    return result;
}

function int64_t
CurrentUndoOrdinal(Buffer *buffer)
{
    return buffer->undo.current_ordinal;
}

function void
MergeUndoHistory(Buffer *buffer, int64_t first_ordinal, int64_t last_ordinal)
{
    if (last_ordinal >= first_ordinal)
    {
        FlushBufferedUndo(buffer);

        UndoNode *node = buffer->undo.at;
        while (node && node->ordinal > last_ordinal)
        {
            node = node->parent;
        }
        while (node && node->ordinal > first_ordinal)
        {
            node->ordinal = first_ordinal;
            node = node->parent;
        }
        if (buffer->undo.current_ordinal == last_ordinal)
        {
            buffer->undo.current_ordinal = first_ordinal + 1;
        }
    }
}

function void
BeginUndoBatch(Buffer *buffer)
{
    if (!buffer->undo_batch_ordinal)
    {
        buffer->undo_batch_ordinal = buffer->undo.current_ordinal;
    }
}

function void
EndUndoBatch(Buffer *buffer)
{
    if (buffer->undo_batch_ordinal)
    {
        MergeUndoHistory(buffer, buffer->undo_batch_ordinal, buffer->undo.current_ordinal);
        buffer->undo_batch_ordinal = 0;
    }
}

function String
BufferSubstring(Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String result = MakeString(range_size, buffer->text + range.start);
    return result;
}

function String
PushBufferRange(Arena *arena, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    String result = PushString(arena, string);
    return result;
}

function String
PushTokenString(Arena *arena, Buffer *buffer, Token *t)
{
    return PushBufferRange(arena, buffer, MakeRangeStartLength(t->pos, t->length));
}

function String
PushBufferRange(StringContainer *container, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    Append(container, string);
    return container->as_string;
}

function String
PushTokenString(StringContainer *container, Buffer *buffer, Token *t)
{
    return PushBufferRange(container, buffer, MakeRangeStartLength(t->pos, t->length));
}

function int64_t
ApplyPositionDelta(int64_t pos, int64_t delta_pos, int64_t delta)
{
    if (pos >= delta_pos)
    {
        if (delta < 0)
        {
            pos += Max(delta_pos - pos, delta);
        }
        else
        {
            pos += delta;
        }
    }
    return pos;
}

function void
OnBufferChanged(Buffer *buffer, int64_t pos, int64_t delta)
{
    for (ViewIterator it = IterateViews(); IsValid(&it); Next(&it))
    {
        View *view = it.view;
        for (Cursor *cursor = IterateCursors(view->id, buffer->id);
             cursor;
             cursor = cursor->next)
        {
            cursor->pos                   = ApplyPositionDelta(cursor->pos,                   pos, delta);
            cursor->selection.inner.start = ApplyPositionDelta(cursor->selection.inner.start, pos, delta);
            cursor->selection.inner.end   = ApplyPositionDelta(cursor->selection.inner.end,   pos, delta);
            cursor->selection.outer.start = ApplyPositionDelta(cursor->selection.outer.start, pos, delta);
            cursor->selection.outer.end   = ApplyPositionDelta(cursor->selection.outer.end,   pos, delta);
        }

        for (uint32_t jump_index = OldestJumpIndex(view); jump_index < view->jump_top; jump_index += 1)
        {
            Jump *jump = GetJump(view, jump_index);
            if (jump->buffer = buffer->id)
            {
                jump->pos = ApplyPositionDelta(jump->pos, pos, delta);
            }
        }
    }
}

function int64_t
BufferReplaceRangeNoUndoHistory(Buffer *buffer, Range range, String text)
{
    if (buffer->flags & Buffer_ReadOnly)
    {
        return range.start;
    }

    buffer->dirty = true;

    LineInfo start_info;
    FindLineInfoByPos(buffer, range.start, &start_info);

    LineInfo end_info;
    FindLineInfoByPos(buffer, range.end, &end_info);

    LineData null_line_data = {};
    LineData *prev_line_data = &null_line_data;
    if (start_info.line > 0)
    {
        // NOTE: Wasteful lookup, could get from the node looked up for start_info directly
        LineInfo prev_line_info;
        FindLineInfoByLine(buffer, start_info.line - 1, &prev_line_info);

        prev_line_data = prev_line_info.data;
    }

    //
    // Delete lines
    // In the case where I modify a single line I still just delete
    // that line and reinsert it instead of being clever with it.
    //

    Range line_range = MakeRange(start_info.line, end_info.line);
    RemoveLinesFromIndex(buffer, line_range);

    //
    // Replace text
    //

    int64_t result = TextStorageReplaceRange(buffer, range, text);
    int64_t delta  = range.start - range.end + (int64_t)text.size;

    int64_t next_retokenize_line = start_info.line;
    int64_t tokenize_pos = start_info.range.start;
    int64_t      end_pos = range.start + delta;

    //
    // Tokenize new lines
    //

    while (tokenize_pos <= end_pos)
    {
        int64_t this_line_start = tokenize_pos;

        LineData line_data;
        tokenize_pos = TokenizeLine(buffer, tokenize_pos, prev_line_data->end_tokenize_state, &line_data);

        LineIndexNode *node = InsertLine(buffer, MakeRange(this_line_start, tokenize_pos), &line_data);
        prev_line_data = &node->data;

        next_retokenize_line += 1;
    }

    //
    // Tokenize other lines as necessary
    // TODO: Cache multiple line states to avoid retokenization cascade
    //

    LineIndexIterator it = IterateLineIndexFromLine(buffer, next_retokenize_line);
    while (IsValid(&it) && it.record->data.start_tokenize_state != prev_line_data->end_tokenize_state)
    {
        LineData *next_line_data = &it.record->data;

        FreeTokens(buffer, it.record);

        TokenizeLine(buffer, it.range.start, prev_line_data->end_tokenize_state, next_line_data);
        prev_line_data = next_line_data;

        Next(&it);
    }

    AssertSlow(ValidateLineIndexFull(buffer));

    OnBufferChanged(buffer, range.start, delta);

    return result;
}

function int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    if (buffer->flags & Buffer_ReadOnly)
    {
        return range.start;
    }

    range = SanitizeRange(range);
    range = ClampRange(range, BufferRange(buffer));

    if (RangeSize(range) == 0 && text.size == 0)
    {
        return range.start;
    }

    String backward = {};
    if (RangeSize(range) > 0)
    {
        backward = PushBufferRange(&buffer->arena, buffer, range);
    }

    String forward = {};
    if (text.size > 0)
    {
        forward = PushString(&buffer->arena, text);
    }

    PushUndo(buffer, range.start, forward, backward);

    if (editor->edit_mode == EditMode_Command)
    {
        FlushBufferedUndo(buffer);
    }

    int64_t result = BufferReplaceRangeNoUndoHistory(buffer, range, text);
    return result;
}

function Token
GetTokenAt(Buffer *buffer, int64_t pos)
{
    LineInfo info;
    FindLineInfoByPos(buffer, pos, &info);

    TokenLocator locator = LocateTokenAtPos(&info, pos);
    return GetToken(locator);
}

function Buffer *
DEBUG_FindWhichBufferThisMemoryBelongsTo(void *memory_init)
{
    char *memory = (char *)memory_init;

    Buffer *result = nullptr;
    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (memory >= buffer->arena.base && memory < buffer->arena.base + buffer->arena.capacity)
        {
            result = buffer;
            break;
        }
    }

    return result;
}
