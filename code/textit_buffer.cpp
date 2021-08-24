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

    while (IsInBufferRange(buffer, inner + 1))
    {
        int64_t newline_length = PeekNewline(buffer, inner + 1);
        if (newline_length)
        {
            outer = inner + newline_length + 1;
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
    FindLineInfoByLine(&buffer->line_index, line, &info);
    return info.range;
}

function Range
GetInnerLineRange(Buffer *buffer, int64_t line)
{
    LineInfo info;
    FindLineInfoByLine(&buffer->line_index, line, &info);

    Range result = info.range;
    if (IsVerticalWhitespaceAscii(ReadBufferByte(buffer, result.end - 1))) result.end -= 1;
    if (IsVerticalWhitespaceAscii(ReadBufferByte(buffer, result.end - 1))) result.end -= 1;

    return result;
}

function void
GetLineRanges(Buffer *buffer, int64_t line, Range *inner, Range *outer)
{
    LineInfo info;
    FindLineInfoByLine(&buffer->line_index, line, &info);

    *outer = info.range;

    *inner = info.range;
    inner->start = FindFirstNonHorzWhitespace(buffer, inner->start);
    if (IsVerticalWhitespaceAscii(ReadBufferByte(buffer, inner->end - 1))) inner->end -= 1;
    if (IsVerticalWhitespaceAscii(ReadBufferByte(buffer, inner->end - 1))) inner->end -= 1;
}

function BufferLocation
CalculateBufferLocationFromPos(Buffer *buffer, int64_t pos)
{
    pos = ClampToBufferRange(buffer, pos);

    BufferLocation result = {};

    LineInfo info;
    FindLineInfoByPos(&buffer->line_index, pos, &info);

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
    FindLineInfoByLine(&buffer->line_index, line, &info);

    result.line       = info.line;
    result.col        = Clamp(col, 0, RangeSize(info.range));
    result.pos        = info.range.start + result.col;
    result.line_range = info.range;

    return result;

#if 0
    if ((line >= 0) &&
        (line < buffer->line_data.count))
    {
        LineData *data = &buffer->line_data[line];
        Range inner_range = MakeRange(data->range.start, data->newline_pos);
        result.line       = line;
        result.col        = Min(col, RangeSize(inner_range));
        result.pos        = ClampToRange(data->range.start + col, inner_range);
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
#endif
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

    platform->DebugPrint("Pushing undo node, ordinal: %lld\n", node->ordinal);

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

    if (!core_config->incremental_parsing)
    {
        TokenizeBuffer(buffer);
        ParseTags(buffer);
    }
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

    if (!core_config->incremental_parsing && !buffer->bulk_edit)
    {
        TokenizeBuffer(buffer);
    }
    else if (core_config->incremental_parsing)
    {
        RetokenizeRange(buffer, range.start, delta);
    }
    ParseTags(buffer);

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

function TokenIterator
MakeTokenIterator(Buffer *buffer, int index, int count = 0)
{
    int max_count = buffer->tokens.count - index;
    if (count <= 0 || count > max_count) count = max_count;

    TokenIterator result = {};
    result.tokens = &buffer->tokens[index];
    result.tokens_end = &result.tokens[count];
    result.token = result.tokens;
    return result;
}

function uint32_t
FindTokenIndexForPos(Buffer *buffer, int64_t pos)
{
    Assert(IsInBufferRange(buffer, pos));

    uint32_t result = 0;

    if (pos != 0)
    {
        int64_t token_count = buffer->tokens.count;
        int64_t lo = 0;
        int64_t hi = token_count - 1;
        while (lo <= hi)
        {
            int64_t index = (lo + hi) / 2;
            Token *t = &buffer->tokens[index];
            if (pos < t->pos)
            {
                hi = index - 1;
            }
            else if (pos >= t->pos + t->length)
            {
                lo = index + 1;
            }
            else
            {
                result = (uint32_t)index;
                break;
            }
        }

        if (lo > hi)
        {
            result = (uint32_t)lo;
        }
    }

    return result;
}

function Token *
GetTokenAt(Buffer *buffer, int64_t pos)
{
    uint32_t index = FindTokenIndexForPos(buffer, pos);
    Token *token = &buffer->tokens[index];
    return token;
}

function TokenIterator
IterateTokens(Buffer *buffer, int64_t pos = 0)
{
    pos = ClampToBufferRange(buffer, pos);

    TokenIterator result = {};
    result.tokens     = buffer->tokens.data;
    result.tokens_end = result.tokens + buffer->tokens.count;
    result.token      = &buffer->tokens[FindTokenIndexForPos(buffer, pos)];

    return result;
}

function TokenIterator
IterateLineTokens(Buffer *buffer, int64_t line)
{
    LineInfo info;
    FindLineInfoByLine(&buffer->line_index, line, &info);

    TokenIterator result = {};
    result.tokens     = &buffer->tokens[info.token_index];
    result.tokens_end = result.tokens + info.token_count;
    result.token      = result.tokens;

    return result;
}
