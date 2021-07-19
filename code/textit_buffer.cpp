function LineEndKind
GuessLineEndKind(String string)
{
    int64_t lf   = 0;
    int64_t crlf = 0;
    for (size_t i = 0; i < string.size;)
    {
        if (string.data[i + 0] == '\r' &&
            string.data[i + 1] == '\n')
        {
            crlf += 1;
            i += 2;
        }
        else if (string.data[i] == '\n')
        {
            lf += 1;
            i += 1;
        }
        else
        {
            i += 1;
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

// taxonomy of a word motion

// #include 
// ^ cursor start
//  ^ selection start
//         ^ cursor/selection end

// ##include
// ^ cursor start
// ^ selection start
//  ^ cursor/selection end

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

function int64_t
FindLineEnd(Buffer *buffer, int64_t pos, bool include_newline = false)
{
    int64_t result = pos;

    while (IsInBufferRange(buffer, result + 1))
    {
        int64_t newline_length = PeekNewline(buffer, result + 1);
        if (newline_length)
        {
            if (include_newline)
            {
                result += newline_length + 1;
            }
            break;
        }
        else
        {
            result += 1;
        }
    }

    return result;
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

function LineData *
GetLineData(Buffer *buffer, int64_t line)
{
    LineData *result = &buffer->null_line_data;
    if (LineIsInBuffer(buffer, line))
    {
        result = &buffer->line_data[line];
    }
    return result;
}

function Range
GetLineRange(Buffer *buffer, int64_t line)
{
    Range range = {};
    if (LineIsInBuffer(buffer, line))
    {
        range = buffer->line_data[line].range;
    }
    return range;
}

function BufferLocation
CalculateBufferLocationFromPos(Buffer *buffer, int64_t pos, LineData **out_line_data = nullptr)
{
    pos = ClampToBufferRange(buffer, pos);

    BufferLocation result = {};

    // TODO: Binary search
    for (int64_t line = 0; line < buffer->line_data.count; line += 1)
    {
        LineData *data = &buffer->line_data[line];
        if ((pos >= data->range.start) &&
            (pos <  data->range.end))
        {
            result.line = line;
            result.pos  = Min(pos, data->whitespace_pos);
            result.col  = result.pos - data->range.start;
            result.line_range = data->range;

            if (out_line_data) *out_line_data = data;

            break;
        }
    }

    return result;
}

function BufferLocation
CalculateBufferLocationFromLineCol(Buffer *buffer, int64_t line, int64_t col)
{
    BufferLocation result = {};
    if ((line >= 0) &&
        (line < buffer->line_data.count))
    {
        LineData *data = &buffer->line_data[line];
        Range whitespace_range = MakeRange(data->range.start, data->whitespace_pos);
        result.line       = line;
        result.col        = Min(col, RangeSize(whitespace_range));
        result.pos        = ClampToRange(data->range.start + col, whitespace_range);
        result.line_range = data->range;

#if 0
        while (IsInBufferRange(buffer, pos) &&
               IsTrailingUtf8Byte(ReadBufferByte(buffer, pos)))
        {
            pos += SignOf(delta.x);
        }
#endif
    }

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

    platform->DebugPrint("Pushing undo node, ordinal: %llu\n", node->ordinal);

    node->pos = pos;
    node->forward = forward;
    node->backward = backward;

    undo->at = node;
    undo->depth += 1;
}

function void
FlushBufferedUndo(Buffer *buffer)
{
    auto undo = &buffer->undo;
    if (undo->run_pos != -1)
    {
        platform->DebugPrint("Flushing undo buffer.\nfwd: '%.*s'\nbck: '%.*s'\n",
                             StringExpand(undo->fwd_buffer),
                             StringExpand(undo->bck_buffer));
        PushUndoInternal(buffer,
                         undo->run_pos,
                         undo->fwd_buffer.AsString(),
                         undo->bck_buffer.AsString());
        undo->fwd_buffer.Clear();
        undo->bck_buffer.Clear();
        undo->run_pos = -1;
        undo->insert_pos = -1;
    }
}

function void
PushUndo(Buffer *buffer, int64_t pos, String forward, String backward)
{
#if 0
    auto undo = &buffer->undo;

    if ((undo->run_pos == -1 || (undo->insert_pos == pos)) &&
        undo->fwd_buffer.CanFitAppend(forward) &&
        (backward.size == 0)) // TODO: Make backward buffering work
    {
        undo->fwd_buffer.Append(forward);
        undo->bck_buffer.Append(backward);
        platform->DebugPrint("Appending to undo buffer.\nfwd: '%.*s'\nbck: '%.*s'\n",
                             StringExpand(undo->fwd_buffer),
                             StringExpand(undo->bck_buffer));
        if (undo->run_pos == -1)
        {
            undo->run_pos = pos;
        }
        undo->insert_pos = pos + forward.size;
    }
    else
#endif
    {
        FlushBufferedUndo(buffer);
        PushUndoInternal(buffer, pos, forward, backward);
    }
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

function uint64_t
CurrentUndoOrdinal(Buffer *buffer)
{
    return buffer->undo.current_ordinal;
}

function void
MergeUndoHistory(Buffer *buffer, uint64_t first_ordinal, uint64_t last_ordinal)
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

function String
BufferSubstring(Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String result = MakeString(range_size, buffer->text + range.start);
    return result;
}

function String
BufferPushRange(Arena *arena, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    String result = PushString(arena, string);
    return result;
}

function int64_t
ApplyPositionDelta(int64_t pos, int64_t delta_pos, int64_t delta)
{
    if (pos >= delta_pos)
    {
        pos += delta;
    }
    return pos;
}

function void
OnBufferChanged(Buffer *buffer, int64_t pos, int64_t delta)
{
    for (size_t i = 0; i < editor_state->view_count; i += 1)
    {
        ViewID view_id = editor_state->used_view_ids[i];
        for (Cursor *cursor = IterateCursors(view_id, buffer->id);
             cursor;
             cursor = cursor->next)
        {
            cursor->pos             = ApplyPositionDelta(cursor->pos, pos, delta);
            cursor->selection.start = ApplyPositionDelta(cursor->selection.start, pos, delta);
            cursor->selection.end   = ApplyPositionDelta(cursor->selection.end, pos, delta);
        }
    }

}

function void
BufferBeginBulkEdit(Buffer *buffer)
{
    Assert(!buffer->bulk_edit);
    buffer->bulk_edit = true;
}

function void
BufferEndBulkEdit(Buffer *buffer)
{
    Assert(buffer->bulk_edit);
    buffer->bulk_edit = false;

    TokenizeBuffer(buffer);
}

function int64_t
BufferReplaceRangeNoUndoHistory(Buffer *buffer, Range range, String text)
{
    if (buffer->flags & Buffer_ReadOnly)
    {
        return range.start;
    }

    int64_t result = TextStorageReplaceRange(buffer, range, text);

    int64_t delta = range.start - range.end + (int64_t)text.size;
    OnBufferChanged(buffer, range.start, delta);

    if (!buffer->bulk_edit)
    {
        TokenizeBuffer(buffer);
    }

    return result;
}

function int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = SanitizeRange(range);
    range = ClampRange(range, BufferRange(buffer));

    String backward = {};
    if (RangeSize(range) > 0)
    {
        backward = BufferPushRange(&buffer->arena, buffer, range);
    }

    String forward = {};
    if (text.size > 0)
    {
        forward = PushString(&buffer->arena, text);
    }

    PushUndo(buffer, range.start, forward, backward);

    if (editor_state->edit_mode == EditMode_Command)
    {
        FlushBufferedUndo(buffer);
    }

    int64_t result = BufferReplaceRangeNoUndoHistory(buffer, range, text);
    return result;
}

