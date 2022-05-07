function Buffer *
OpenNewBuffer(String buffer_name, BufferFlags flags)
{
    BufferID id = {};
    if (editor->buffer_count < MAX_BUFFER_COUNT)
    {
        id = editor->free_buffer_ids[MAX_BUFFER_COUNT - editor->buffer_count - 1];
        id.generation += 1;

        editor->used_buffer_ids[editor->buffer_count] = id;
        editor->buffer_count += 1;
    }

    Buffer *result = BootstrapPushStruct(Buffer, arena);
    result->id                   = id;
    result->flags                = flags;
    result->name                 = PushString(&result->arena, buffer_name);
    result->undo.at              = &result->undo.root;
    result->undo.run_pos         = -1;
    result->undo.insert_pos      = -1;
    result->undo.current_ordinal = 1;
    result->indent_rules         = &editor->default_indent_rules;
    result->language             = &language_registry->null_language;
    result->tags                 = PushStruct(&result->arena, Tags);
    result->heap                 = platform->CreateHeap(Kilobytes(4), 0);
    DllInit(&result->tags->sentinel);

    result->last_save_undo_ordinal = result->undo.current_ordinal;

    AllocateTextStorage(result, TEXTIT_BUFFER_SIZE);

    editor->buffers[result->id.index] = result;

    return result;
}

function bool
DestroyBuffer(BufferID id)
{
    Buffer *buffer = GetBuffer(id);
    if (IsNullBuffer(buffer))
    {
        return false;
    }

    if (buffer->flags & Buffer_Indestructible)
    {
        return false;
    }

    RemoveProjectAssociation(buffer);
    FreeAllTags(buffer);

    size_t used_id_index = 0;
    for (size_t i = 0; i < editor->buffer_count; i += 1)
    {
        BufferID test_id = editor->used_buffer_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor->free_buffer_ids[MAX_BUFFER_COUNT - editor->buffer_count] = id;
    editor->used_buffer_ids[used_id_index] = editor->used_buffer_ids[--editor->buffer_count];

    for (size_t i = 0; i < editor->view_count; i += 1)
    {
        ViewID view_id = editor->used_view_ids[i];
        DestroyCursors(view_id, id);
    }

    editor->buffers[id.index] = nullptr;

    Release(&buffer->arena);
    platform->DestroyHeap(buffer->heap);

    return true;
}

function Buffer *
BeginOpenBufferFromFile(String filename, BufferFlags flags, bool *already_exists = nullptr)
{
    if (already_exists) *already_exists = false;

    ScopedMemory temp;
    String full_path = platform->PushFullPath(temp, filename);

    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (AreEqual(buffer->full_path, full_path))
        {
            if (already_exists) *already_exists = true;
            return buffer;
        }
    }

    String leaf;
    SplitPath(filename, &leaf);

    Buffer *buffer = OpenNewBuffer(leaf);

    buffer->full_path = PushString(&buffer->arena, full_path);
    buffer->flags |= flags;

    AssociateProject(buffer);

    return buffer;
}

function void
FinalizeOpenBufferFromFile(Buffer *buffer)
{
    //
    // All work done in this function must be threadsafe, as of writing
    // it just concerns itself with the provided buffer and therefore
    // doesn't need any synchronization. All the memory for it is allocated
    // from the buffer's arena. -06/09/2021
    //

    size_t file_size = platform->GetFileSize(buffer->full_path);
    EnsureSpace(buffer, file_size + 1); // + 1 for null terminator but this is fucking jank I want this code to die
    if (platform->ReadFileInto(TEXTIT_BUFFER_SIZE, buffer->text, buffer->full_path) != file_size)
    {
        INVALID_CODE_PATH;
    }
    buffer->count = (int64_t)file_size;
    buffer->line_end = GuessLineEndKind(MakeString(buffer->count, (uint8_t *)buffer->text));

    String ext;
    SplitExtension(buffer->full_path, &ext);

    for (LanguageSpec *spec = language_registry->first_language; spec; spec = spec->next)
    {
        for (int extension_index = 0; extension_index < spec->associated_extension_count; extension_index += 1)
        {
            if (AreEqual(spec->associated_extensions[extension_index], ext))
            {
                buffer->language          = spec;
                buffer->inferred_language = buffer->language;
                break;
            }
        }
    }

    TokenizeBuffer(buffer);
    ParseTags(buffer);
}

function
PLATFORM_JOB(FinalizeOpenBufferFromFileJob)
{
    Buffer *buffer = (Buffer *)userdata;
    FinalizeOpenBufferFromFile(buffer);
}

function Buffer *
OpenBufferFromFile(String filename, BufferFlags flags)
{
    bool already_exists;
    Buffer *buffer = BeginOpenBufferFromFile(filename, flags, &already_exists);
    if (!already_exists)
    {
        FinalizeOpenBufferFromFile(buffer);
    }
    return buffer;
}

function Buffer *
OpenBufferFromFileAsync(PlatformJobQueue *queue, String filename, BufferFlags flags)
{
    bool already_exists;
    Buffer *buffer = BeginOpenBufferFromFile(filename, flags, &already_exists);
    if (!already_exists)
    {
        platform->AddJob(queue, buffer, FinalizeOpenBufferFromFileJob);
    }
    return buffer;
}

function Buffer *
GetBuffer(BufferID id)
{
    Buffer *result = editor->buffers[0];
    if (id.index > 0 && id.index < MAX_BUFFER_COUNT)
    {
        Buffer *buffer = editor->buffers[id.index];
        if (buffer && buffer->id == id)
        {
            result = buffer;
        }
    }
    return result;
}

function bool
IsNullBuffer(Buffer *buffer)
{
    return buffer != editor->null_buffer;
}

function BufferIterator
IterateBuffers(void)
{
    BufferIterator result = {};
    result.index  = 1;
    result.buffer = GetBuffer(editor->used_buffer_ids[result.index]);
    return result;
}

function bool
IsValid(BufferIterator *iter)
{
    return (iter->index < editor->buffer_count);
}

function void
Next(BufferIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor->buffer_count)
    {
        iter->buffer = GetBuffer(editor->used_buffer_ids[iter->index]);
    }
}

function Buffer *
GetActiveBuffer(void)
{
    return GetBuffer(GetView(editor->active_window->view)->buffer);
}

function Range
BufferRange(Buffer *buffer)
{
    return MakeRange(0, buffer->count);
}

function bool
IsInBufferRange(Buffer *buffer, int64_t pos)
{
    bool result = ((pos >= 0) && (pos < buffer->count));
    return result;
}

function int64_t
ClampToBufferRange(Buffer *buffer, int64_t pos)
{
    if (pos >= buffer->count) pos = buffer->count - 1;
    if (pos < 0) pos = 0;
    return pos;
}

function bool
LineIsInBuffer(Buffer *buffer, int64_t line)
{
    return ((line >= 0) && (line < GetLineCount(buffer)));
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
ScanWordForward(Buffer *buffer, int64_t pos)
{
    Selection result = {};
    result.inner = MakeRange(pos);
    result.outer = MakeRange(pos);

    bool skipped_whitespace = false;
    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        skipped_whitespace = true;
        pos += 1;
    }

    if (skipped_whitespace) 
    {
        result.inner.end = result.outer.end = pos;
        return result;
    }

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
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    Selection result = {};
    result.inner = MakeRange(pos);
    result.outer = MakeRange(pos);

    if (IsInBufferRange(buffer, pos - 1))
    {
        pos -= 1;
    }

    bool skipped_whitespace = false;
    while (IsInBufferRange(buffer, pos) &&
           IsWhitespaceAscii(ReadBufferByte(buffer, pos)))
    {
        skipped_whitespace = true;
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

#if 0
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
#endif

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

function void
FindLineEnd(Buffer *buffer, int64_t pos, int64_t *out_inner, int64_t *out_outer)
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

    if (out_inner) *out_inner = inner;
    if (out_outer) *out_outer = outer;
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
EncloseLine(Buffer *buffer, int64_t pos, bool including_newline)
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
    result.col        = ClampToRange(col, MakeRange(0, info.newline_pos - info.range.start));
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

// function String
// BufferSubstring(Buffer *buffer, Range range)
// {
//     int64_t range_size = range.end - range.start;
//     String result = MakeString(range_size, buffer->text + range.start);
//     return result;
// }

function String
PushBufferRange(Arena *arena, Buffer *buffer, Range range)
{
    int64_t range_size = range.end - range.start;
    String string = MakeString(range_size, buffer->text + range.start);
    String result = PushString(arena, string);
    return result;
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
PushTokenString(Arena *arena, Buffer *buffer, Token *t)
{
    return PushBufferRange(arena, buffer, MakeRangeStartLength(t->pos, t->length));
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

function Range
FixBufferRangeAfterEdit(Range range, int64_t edit_pos, int64_t edit_delta)
{
    Range result;
    result.start = ApplyPositionDelta(range.start, edit_pos, edit_delta);
    result.end   = ApplyPositionDelta(range.end  , edit_pos, edit_delta);
    return result;
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

		// TODO: Test if this behaves correctly
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
    // int64_t      end_pos = range.start + delta;

    int lines_to_retokenize = 1 + CountNewlines(text);

    //
    // Tokenize new lines
    //

    while (lines_to_retokenize > 0)
    {
        int64_t this_line_start = tokenize_pos;

        LineData line_data;
        tokenize_pos = TokenizeLine(buffer, tokenize_pos, prev_line_data->end_tokenize_state, &line_data);

        LineIndexNode *node = InsertLine(buffer, MakeRange(this_line_start, tokenize_pos), line_data);
        prev_line_data = &node->data;

        next_retokenize_line += 1;
        lines_to_retokenize -= 1;
    }

    //
    // Tokenize other lines as necessary
    // TODO: Cache multiple line states to avoid retokenization cascade
    //

    LineIndexIterator it = IterateLineIndexFromLine(buffer, next_retokenize_line);
    while (IsValid(&it) && it.record->data.start_tokenize_state != prev_line_data->end_tokenize_state)
    {
        LineData *next_line_data = &it.record->data;

        TokenBlock *old_prev = next_line_data->first_token_block->prev;
        TokenBlock *old_next = next_line_data->last_token_block->next;

        FreeTokens(buffer, it.record);
        TokenizeLine(buffer, it.range.start, prev_line_data->end_tokenize_state, next_line_data);

        next_line_data->first_token_block->prev = old_prev;
        next_line_data->last_token_block->next  = old_next;
        if (old_prev) old_prev->next = next_line_data->first_token_block;
        if (old_next) old_next->prev = next_line_data->last_token_block;

        prev_line_data = next_line_data;

        Next(&it);
    }

    AssertSlow(ValidateLineIndexFull(buffer));
    AssertSlow(ValidateTokenIteration(buffer));

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

function void 
DoBulkEdit(Buffer *buffer, Slice<BulkEdit> edits)
{
    for (BulkEdit &edit: edits)
    {
        edit.range = SanitizeRange(edit.range);
    }

    Sort(edits.count, edits.data, +[](const BulkEdit &a, const BulkEdit &b) {
        return a.range.start < b.range.start;
    });

    // Merge overlapping ranges
    size_t dst = 0;
    size_t src = 1;
    while (src < edits.count)
    {
        Range a = edits[dst].range;
        Range b = edits[src].range;

        if (edits[src].string.size == 0 && edits[dst].string.size == 0 && RangesOverlap(a, b))
        {
            edits[dst].range = Union(a, b);
        }
        else
        {
            dst++;
        }
        src++;
    }
    edits.count = dst + 1;

    // Do the edit
    BeginUndoBatch(buffer);
    for (size_t edit_index = 0; edit_index < edits.count; edit_index++)
    {
        int64_t edit_pos = edits[edit_index].range.start;
        int64_t edit_delta = edits[edit_index].range.start - edits[edit_index].range.end + edits[edit_index].string.size;

        // replace text
        BufferReplaceRange(buffer, edits[edit_index].range, edits[edit_index].string);

        // SPEED: Quadratic!
        for (size_t adjust_index = edit_index + 1; adjust_index < edits.count; adjust_index++)
        {
            edits[adjust_index].range = FixBufferRangeAfterEdit(edits[adjust_index].range, edit_pos, edit_delta);
        }
    }
    EndUndoBatch(buffer);
}

function Range
FindNextOccurrence(Buffer *buffer, int64_t pos, String query, StringMatchFlags flags)
{
    pos = ClampToBufferRange(buffer, pos);
    Range result = MakeRange(buffer->count);

    String text = MakeString(buffer->count, buffer->text);
    text = Advance(text, pos);

    size_t found_pos = FindSubstring(text, query, flags);
    if (found_pos != text.size)
    {
        int64_t real_pos = pos + (int64_t)found_pos;
        result = MakeRangeStartLength(real_pos, (int64_t)query.size);
    }

    return result;
}

function Range
FindPreviousOccurrence(Buffer *buffer, int64_t pos, String query, StringMatchFlags flags)
{
    pos = ClampToBufferRange(buffer, pos);
    Range result = MakeRange(pos);

    String text = MakeString(pos, buffer->text);

    size_t found_pos = FindSubstringBackward(text, query, flags);
    if (found_pos != text.size)
    {
        int64_t real_pos = found_pos;
        result = MakeRangeStartLength(real_pos, (int64_t)query.size);
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

    node->ordinal = undo->current_ordinal;
    undo->current_ordinal += 1;

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
    if (last_ordinal > first_ordinal)
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

function bool
HasUnsavedChanges(Buffer *buffer)
{
    return buffer->last_save_undo_ordinal != CurrentUndoOrdinal(buffer);
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

//
// Line index
//

function LineIndexNode *
AllocateLineIndexNode(Buffer *buffer, LineIndexNodeKind kind)
{
    if (!buffer->first_free_line_index_node)
    {
        buffer->first_free_line_index_node = PushStructNoClear(&buffer->arena, LineIndexNode);
        buffer->first_free_line_index_node->next = nullptr;
    }
    LineIndexNode *result = SllStackPop(buffer->first_free_line_index_node);
    ZeroStruct(result);
    result->kind = kind;
    return result;
}

function void
FreeTokens(Buffer *buffer, LineIndexNode *node)
{
    Assert(node->kind == LineIndexNode_Record);
    while (TokenBlock *block = node->data.first_token_block)
    {
        node->data.first_token_block = block->next;
        FreeTokenBlock(buffer, block);
        if (block == node->data.last_token_block)
        {
            break;
        }
    }
}

function void
FreeLineIndexNode(Buffer *buffer, LineIndexNode *node)
{
    if (node->kind == LineIndexNode_Record)
    {
        FreeTokens(buffer, node);
    }
    node->kind = LineIndexNode_FREE;
    SllStackPush(buffer->first_free_line_index_node, node);
}

struct LineIndexLocator
{
    LineIndexNode *record;
    int64_t        pos;
    int64_t        line;
    int64_t        times_recursed;
};

template <bool by_line>
function void
LocateLineIndexNode(LineIndexNode    *node,      
                    int64_t           target,     
                    LineIndexLocator *locator,   
                    int64_t           offset      = 0, 
                    int64_t           line_offset = 0,
                    int64_t           times_recursed = 0)
{
    Assert(node->kind != LineIndexNode_Record);

    int next_index = 0;
    for (; next_index < node->entry_count - 1; next_index += 1)
    {
        LineIndexNode *child = node->children[next_index];

        int64_t delta;
        if constexpr(by_line)
        {
            delta = target - line_offset - child->line_span;
        }
        else
        {
            delta = target - offset - child->span;
        }

        if (delta < 0)
        {
            break;
        }

        offset      += child->span;
        line_offset += child->line_span;
    }

    LineIndexNode *child = node->children[next_index];
    if (child->kind == LineIndexNode_Record)
    {
        ZeroStruct(locator);
        locator->record = child;
        locator->pos    = offset;
        locator->line   = line_offset;
        locator->times_recursed = times_recursed;
        return;
    }

    LocateLineIndexNode<by_line>(child, target, locator, offset, line_offset, times_recursed + 1);
}

function void
LocateLineIndexNodeByPos(LineIndexNode *node, int64_t pos, LineIndexLocator *locator)
{
    LocateLineIndexNode<false>(node, pos, locator);
}

function void
LocateLineIndexNodeByLine(LineIndexNode *node, int64_t line, LineIndexLocator *locator)
{
    LocateLineIndexNode<true>(node, line, locator);
}

template <bool by_line>
function void
FindLineInfo(Buffer *buffer, int64_t target, LineInfo *out_info)
{
    PlatformHighResTime start = platform->GetTime();

    LineIndexLocator locator;
    LocateLineIndexNode<by_line>(buffer->line_index_root, target, &locator);

    LineIndexNode *node = locator.record;
    Assert(node->kind == LineIndexNode_Record);

    LineData *data = &node->data;

    ZeroStruct(out_info);
    out_info->line        = locator.line;
    out_info->range       = MakeRangeStartLength(locator.pos, node->span);
    out_info->newline_pos = locator.pos + data->newline_col;
    out_info->data        = data;

    PlatformHighResTime end = platform->GetTime();
    editor->debug.line_index_lookup_timing += platform->SecondsElapsed(start, end);

    editor->debug.line_index_lookup_count += 1;
    editor->debug.line_index_lookup_recursion_count += locator.times_recursed;
}

function void
FindLineInfoByPos(Buffer *buffer, int64_t pos, LineInfo *out_info)
{
    FindLineInfo<false>(buffer, pos, out_info);
}

function void
FindLineInfoByLine(Buffer *buffer, int64_t line, LineInfo *out_info)
{
    FindLineInfo<true>(buffer, line, out_info);
}

function void
RemoveRecord(LineIndexNode *record)
{
    Assert(record->kind == LineIndexNode_Record);

    LineIndexNode *leaf = record->parent;
    Assert(leaf->kind == LineIndexNode_Leaf);

    // Find the record's index
    int entry_index = 0;
    for (; entry_index < leaf->entry_count && leaf->children[entry_index] != record; entry_index += 1);

    Assert(entry_index < leaf->entry_count);
    Assert(leaf->children[entry_index] == record);

    if (record->prev)
    {
        record->prev->next = record->next;
        record->prev->data.last_token_block->next = record->data.last_token_block->next;
    }

    if (record->next)
    {
        record->next->prev = record->prev;
        record->next->data.first_token_block->prev = record->data.first_token_block->prev;
    }

#if VALIDATE_LINE_INDEX_EXTRA_SLOW
    ValidateTokenBlockChain(record);
#endif

    for (LineIndexNode *parent = leaf; parent; parent = parent->parent)
    {
        parent->span      -= record->span;
        parent->line_span -= 1;
    }

    for (int i = entry_index; i < leaf->entry_count - 1; i += 1)
    {
        leaf->children[i] = leaf->children[i + 1];
    }
    leaf->entry_count -= 1;

#if TEXTIT_SLOW
    {
        LineIndexNode *root = record;
        for (; root->parent; root = root->parent);
        ValidateLineIndexTreeIntegrity(root);
    }
#endif
}

#if 0
function LineIndexNode *
FindRelativeRecord(LineIndexNode *leaf, int index, int offset)
{
    LineIndexNode *result = nullptr;

    int new_index = index + offset;
    while (leaf && new_index 
}
#endif

function LineIndexNode *
SplitNode(Buffer *buffer, LineIndexNode *node)
{
    int8_t  left_count = LINE_INDEX_ORDER;
    int8_t right_count = LINE_INDEX_ORDER + 1;

    LineIndexNode *l1 = node;
    LineIndexNode *l2 = AllocateLineIndexNode(buffer, l1->kind);

    l1->entry_count = left_count;
    l2->entry_count = right_count;

    l1->span      = 0;
    l1->line_span = 0;
    for (int i = 0; i < left_count; i += 1)
    {
        l1->span      += l1->children[i]->span;
        l1->line_span += l1->children[i]->line_span;
    }

    l2->span      = 0;
    l2->line_span = 0;
    for (int i = 0; i < right_count; i += 1)
    {
        l2->children[i] = l1->children[left_count + i];
        l2->children[i]->parent = l2;
        l2->span      += l2->children[i]->span;
        l2->line_span += l2->children[i]->line_span;
    }

    l2->prev = l1;
    l2->next = l1->next;
    l2->prev->next = l2;
    if (l2->next) l2->next->prev = l2;

    Assert(l1->next == l2);
    Assert(l2->prev == l1);
    if (l1->prev) Assert(l1->prev->next == l1);
    if (l2->next) Assert(l2->next->prev == l2);

    return l2;
}

function LineIndexNode *
InsertEntry(Buffer         *buffer,
            LineIndexNode  *node,        
            int64_t         pos,
            LineIndexNode  *record,       
            int64_t offset = 0)
{
    LineIndexNode *result = nullptr;

    int64_t span = record->span;
    node->span      += span;
    node->line_span += 1;

    if (node->kind == LineIndexNode_Leaf)
    {
        int insert_index = 0;
        for (int i = 0; i < node->entry_count; i += 1)
        {
            int64_t test_span = node->children[i]->span;
            if (pos - offset - test_span < 0)
            {
                break;
            }
            insert_index += 1;
            offset += test_span;
        }

        // TODO: This looks a bit silly doesn't it
        if (insert_index < node->entry_count)
        {
            LineIndexNode *next_child = node->children[insert_index];
            record->prev = next_child->prev;
            record->next = next_child;

            record->data.first_token_block->prev = next_child->data.first_token_block->prev;
            record->data.last_token_block->next  = next_child->data.first_token_block;
        }
        else if (node->entry_count > 0)
        {
            LineIndexNode *prev_child = node->children[insert_index - 1];
            record->prev = prev_child;
            record->next = prev_child->next;

            record->data.first_token_block->prev = prev_child->data.last_token_block;
            record->data.last_token_block->next  = prev_child->data.last_token_block->next;
        }

        if (record->next) record->next->prev = record;
        if (record->prev) record->prev->next = record;

        if (record->data.last_token_block->next)  record->data.last_token_block->next->prev  = record->data.last_token_block;
        if (record->data.first_token_block->prev) record->data.first_token_block->prev->next = record->data.first_token_block;

#if VALIDATE_LINE_INDEX_EXTRA_SLOW
        ValidateTokenBlockChain(record);
#endif

        for (int i = node->entry_count; i > insert_index; i -= 1)
        {
            node->children[i] = node->children[i - 1];
        }

        record->parent = node;
        node->children[insert_index] = record;
        node->entry_count += 1;

        if (node->entry_count > 2*LINE_INDEX_ORDER)
        {
            result = SplitNode(buffer, node);

#if VALIDATE_LINE_INDEX_EXTRA_SLOW
            ValidateTokenBlockChain(record);
#endif
        }
    }
    else
    {
        int insert_index = 0;
        for (int i = 0; i < node->entry_count - 1; i += 1)
        {
            int64_t test_span = node->children[i]->span;
            if (pos - offset - test_span < 0)
            {
                break;
            }
            insert_index += 1;
            offset += test_span;
        }

        LineIndexNode *child = node->children[insert_index];
        if (LineIndexNode *split = InsertEntry(buffer, child, pos, record, offset))
        {
            for (int i = node->entry_count; i > insert_index; i -= 1)
            {
                node->children[i] = node->children[i - 1];
            }

            split->parent = node;
            node->children[insert_index + 1] = split;
            node->entry_count += 1;

            node->span      = 0;
            node->line_span = 0;
            for (int i = 0; i < node->entry_count; i += 1)
            {
                node->span      += node->children[i]->span;
                node->line_span += node->children[i]->line_span;
            }

            if (node->entry_count > 2*LINE_INDEX_ORDER)
            {
                result = SplitNode(buffer, node);
            }
        }
    }
    return result;
}

function LineIndexNode *
GetFirstLeafNode(LineIndexNode *root)
{
    LineIndexNode *result = root;
    while (result->kind != LineIndexNode_Leaf)
    {
        Assert(result->entry_count > 0);
        result = result->children[0];
    }
    Assert(result);
    return result;
}

function LineIndexNode *
GetFirstRecord(LineIndexNode *root)
{
    LineIndexNode *result = root;
    while (result->kind != LineIndexNode_Record)
    {
        Assert(result->entry_count > 0);
        result = result->children[0];
    }
    Assert(result);
    return result;
}

function LineIndexNode *
InsertLine(Buffer *buffer, Range range, const LineData &data)
{
    PlatformHighResTime start = platform->GetTime();

    if (!buffer->line_index_root)
    {
        buffer->line_index_root = AllocateLineIndexNode(buffer, LineIndexNode_Leaf);
    }

    LineIndexNode *record = AllocateLineIndexNode(buffer, LineIndexNode_Record);
    record->span      = RangeSize(range);
    record->line_span = 1;
    record->data = data;

    if (LineIndexNode *split_node = InsertEntry(buffer, buffer->line_index_root, range.start, record))
    {
        LineIndexNode *new_root = AllocateLineIndexNode(buffer, LineIndexNode_Internal);
        new_root->entry_count = 2;
        new_root->children[0] = buffer->line_index_root;
        new_root->children[1] = split_node;
        new_root->children[0]->parent = new_root;
        new_root->children[1]->parent = new_root;
        new_root->span      += new_root->children[0]->span;
        new_root->span      += new_root->children[1]->span;
        new_root->line_span += new_root->children[0]->line_span;
        new_root->line_span += new_root->children[1]->line_span;
        buffer->line_index_root = new_root;
    }

    // NOTE: I am not doing the full validation here because the buffer may be desynced with the line index temporarily
    AssertSlow(ValidateLineIndexTreeIntegrity(buffer->line_index_root));

    PlatformHighResTime end = platform->GetTime();
    editor->debug.line_index_insert_timing += platform->SecondsElapsed(start, end);

    return record;
}

function void
ClearLineIndex(Buffer *buffer, LineIndexNode *node)
{
    if (!node) return;

    for (int i = 0; i < node->entry_count; i += 1)
    {
        ClearLineIndex(buffer, node->children[i]);
    }
    FreeLineIndexNode(buffer, node);
}

function void
ClearLineIndex(Buffer *buffer)
{
    ClearLineIndex(buffer, buffer->line_index_root);
    buffer->line_index_root = nullptr;
}

function void
CountLineIndex(LineIndexNode *node, LineIndexCountResult *result)
{
    if (!node) return;

    result->nodes      += 1;
    result->nodes_size += sizeof(*node);

    if (node->kind == LineIndexNode_Record)
    {
        for (TokenBlock *block = node->data.first_token_block;
             block;
             block = block->next)
        {
            result->token_blocks           += 1;
            result->token_blocks_size      += sizeof(*block);
            result->token_blocks_capacity  += ArrayCount(block->tokens);
            result->token_blocks_occupancy += block->token_count;

            if (block == node->data.last_token_block)
            {
                break;
            }
        }
    }

    for (int i = 0; i < node->entry_count; i += 1)
    {
        CountLineIndex(node->children[i], result);
    }
}


//
// Line Index Iterator
//

function LineIndexIterator
IterateLineIndex(Buffer *buffer)
{
    LineIndexIterator result = {};
    result.record = GetFirstRecord(buffer->line_index_root);
    result.range  = MakeRangeStartLength(0, result.record->span);
    result.line   = 0;
    return result;
}

function LineIndexIterator
IterateLineIndexFromPos(Buffer *buffer, int64_t pos)
{
    LineIndexLocator locator;
    LocateLineIndexNodeByPos(buffer->line_index_root, pos, &locator);

    LineIndexIterator result = {};
    result.record = locator.record;
    result.range  = MakeRangeStartLength(locator.pos, result.record->span);
    result.line   = locator.line;

    return result;
}

function LineIndexIterator
IterateLineIndexFromLine(Buffer *buffer, int64_t line)
{
    LineIndexLocator locator;
    LocateLineIndexNodeByLine(buffer->line_index_root, line, &locator);

    LineIndexIterator result = {};
    result.record = locator.record;
    result.range  = MakeRangeStartLength(locator.pos, result.record->span);
    result.line   = locator.line;

    return result;
}

function bool
IsValid(LineIndexIterator *it)
{
    return !!it->record;
}

function void
Next(LineIndexIterator *it)
{
    if (!IsValid(it)) return;

    it->record = it->record->next;
    it->line += 1;

    if (!IsValid(it)) return;

    it->range.start = it->range.end;
    it->range.end   = it->range.start + it->record->span;
}

function void
Prev(LineIndexIterator *it)
{
    if (!IsValid(it)) return;

    it->record = it->record->prev;
    it->line -= 1;

    if (!IsValid(it)) return;

    it->range.end   = it->range.start;
    it->range.start = it->range.end - it->record->span;
}

function LineIndexNode *
RemoveCurrent(LineIndexIterator *it)
{
    LineIndexNode *to_remove = it->record;

    // Fix up the iterator to account for the removed record
    it->range.end = it->range.start + to_remove->span;
    it->record = it->record->next;

    RemoveRecord(to_remove);
    return to_remove;
}

function void
GetLineInfo(LineIndexIterator *it, LineInfo *out_info)
{
    ZeroStruct(out_info);
    out_info->line        = it->line;
    out_info->range       = it->range;
    out_info->newline_pos = it->record->data.newline_col - it->range.start;
    out_info->flags       = it->record->data.flags;
    out_info->data        = &it->record->data;
}

function void
AdjustSize(LineIndexNode *record, int64_t span_delta)
{
    Assert(record->kind == LineIndexNode_Record);
    for (LineIndexNode *node = record; node; node = node->parent)
    {
        node->span += span_delta;
        Assert(node->span >= 0);
    }
}

function int64_t
GetLineCount(Buffer *buffer)
{
    return buffer->line_index_root ? buffer->line_index_root->line_span : 0;
}

function void
RemoveLinesFromIndex(Buffer *buffer, Range line_range)
{
    LineIndexIterator it = IterateLineIndexFromLine(buffer, line_range.start);

    int64_t line_count = RangeSize(line_range) + 1;

    for (int64_t i = 0; i < line_count; i += 1)
    {
        LineIndexNode *node = RemoveCurrent(&it);
        FreeLineIndexNode(buffer, node);
    }

    // NOTE: I am not doing the full validation here because the buffer may be desynced with the line index temporarily
    AssertSlow(ValidateLineIndexTreeIntegrity(buffer->line_index_root));
}

function void
MergeLines(Buffer *buffer, Range range)
{
    LineIndexIterator it = IterateLineIndexFromPos(buffer, range.start);

    int64_t size = RangeSize(range);
    if (size <= 0) return;

    LineIndexNode *merge_head = nullptr;
    if (range.start > it.range.start)
    {
        merge_head = it.record;

        int64_t offset = range.start - it.range.start;
        int64_t delta = Min(it.record->span - offset, size);
        size -= delta;

        AdjustSize(it.record, -delta);
        Next(&it);
    }

    while (size >= it.record->span)
    {
        LineIndexNode *removed = RemoveCurrent(&it);
        size -= removed->span;
        FreeLineIndexNode(buffer, removed);
    }

    if (size > 0)
    {
        if (merge_head)
        {
            LineIndexNode *merge_tail = RemoveCurrent(&it);
            AdjustSize(merge_head, merge_tail->span - size);
            FreeLineIndexNode(buffer, merge_tail);
        }
        else
        {
            AdjustSize(it.record, -size);
        }
    }

    // NOTE: I am not doing the full validation here because the buffer may be desynced with the line index temporarily
    AssertSlow(ValidateLineIndexTreeIntegrity(buffer->line_index_root));
}

function bool
ValidateTokenBlockChain(LineIndexNode *record)
{
    int visited_block_count = 0;

    ScopedMemory temp;
    TokenBlock **big_horrid_stack = PushArrayNoClear(temp, 10'000, TokenBlock *);

    bool is_first_node = !record->prev;
    bool is_last_node  = !record->next;

    TokenBlock *first = record->data.first_token_block;
    TokenBlock *block = first;
    for (; block; block = block->next)
    {
        Assert(block->token_count != TOKEN_BLOCK_FREE_TAG);
        for (int i = 0; i < visited_block_count; i += 1)
        {
            Assert(big_horrid_stack[i] != block);
        }
        big_horrid_stack[visited_block_count++] = block;

        if (!is_first_node || block != record->data.first_token_block)
        {
            Assert(block->prev);
        }

        if (!is_last_node || block != record->data.last_token_block)
        {
            Assert(block->next);
        }

        if (block == record->data.last_token_block)
        {
            break;
        }
    }

    Assert(block == record->data.last_token_block);

    visited_block_count = 0;

    for (; block; block = block->prev)
    {
        Assert(block->token_count != TOKEN_BLOCK_FREE_TAG);
        for (int i = 0; i < visited_block_count; i += 1)
        {
            Assert(big_horrid_stack[i] != block);
        }
        big_horrid_stack[visited_block_count++] = block;

        if (!is_first_node || block != record->data.first_token_block)
        {
            Assert(block->prev);
        }

        if (!is_last_node || block != record->data.last_token_block)
        {
            Assert(block->next);
        }

        if (block == record->data.first_token_block)
        {
            break;
        }
    }

    return true;
}

function bool
ValidateLineIndexInternal(LineIndexNode *root, Buffer *buffer)
{
    UNUSED_VARIABLE(root);
    UNUSED_VARIABLE(buffer);

#if VALIDATE_LINE_INDEX
    //
    // validate all spans sum up correctly
    // and newlines are where we expect: at the ends of lines
    //
    {
        int stack_at = 0;
        LineIndexNode *stack[256];

        stack[stack_at++] = root;

        int64_t pos  = 0;
        int64_t line = 0;
        while (stack_at > 0)
        {
            LineIndexNode *node = stack[--stack_at];
            Assert(node->kind != LineIndexNode_FREE);

            if (node->kind == LineIndexNode_Record)
            {
                LineData *data = &node->data;
                int64_t newline_size = node->span - data->newline_col;

                Assert(newline_size == 1 || newline_size == 2);

                if (buffer)
                {
                    for (int64_t i = 0; i < node->span - newline_size; i += 1)
                    {
                        Assert(!IsVerticalWhitespaceAscii(ReadBufferByte(buffer, pos + i)));
                    }

                    if (newline_size == 2)
                    {
                        Assert(ReadBufferByte(buffer, pos + data->newline_col)     == '\r');
                        Assert(ReadBufferByte(buffer, pos + data->newline_col + 1) == '\n');
                    }
                    else if (newline_size == 1)
                    {
                        Assert(ReadBufferByte(buffer, pos + data->newline_col)     == '\n');
                        Assert(ReadBufferByte(buffer, pos + data->newline_col - 1) != '\r');
                    }
                    else
                    {
                        INVALID_CODE_PATH;
                    }

                    bool is_first_node = !node->prev;
                    bool is_last_node  = !node->next;

                    for (TokenBlock *block = data->first_token_block;
                         block;
                         block = block->next)
                    {
                        if (!is_first_node || block != data->first_token_block)
                        {
                            Assert(block->prev);
                        }

                        if (!is_last_node || block != data->last_token_block)
                        {
                            Assert(block->next);
                        }

                        Assert(block->token_count != TOKEN_BLOCK_FREE_TAG);
                        if (block == data->last_token_block)
                        {
                            break;
                        }
                    }
                }

                pos  += node->span;
                line += 1;
            }
            else
            {
                int64_t sum      = 0;
                int64_t line_sum = 0;
                for (int i = node->entry_count - 1 ; i >= 0; i -= 1)
                {
                    LineIndexNode *child = node->children[i];
                    sum      += child->span;
                    line_sum += child->line_span;

                    Assert(child->parent == node);
                    Assert(child->parent->children[i] == child);

                    Assert(stack_at < ArrayCount(stack));
                    stack[stack_at++] = child;
                }

                Assert(sum      == node->span);
                Assert(line_sum == node->line_span);
            }
        }
    }

    //
    // validate the linked list of nodes links up properly
    //
    {
        for (LineIndexNode *level = root;
             level->entry_count > 0;
             level = level->children[0])
        {
            int64_t sum      = 0;
            int64_t line_sum = 0;

            // forward
            LineIndexNode *first = level->children[0];
            LineIndexNode *last  = nullptr;
            for (LineIndexNode *node = first;
                 node;
                 node = node->next)
            {
                Assert(node->kind == first->kind);
                if (node->prev) Assert(node->prev->next == node);
                if (node->next) Assert(node->next->prev == node);
                sum      += node->span;
                line_sum += node->line_span;
                if (!node->next)
                {
                    last = node;
                }
            }
            Assert(sum      == root->span);
            Assert(line_sum == root->line_span);

            // and back
            sum      = 0;
            line_sum = 0;
            for (LineIndexNode *node = last;
                 node;
                 node = node->prev)
            {
                Assert(node->kind == last->kind);
                if (node->prev) Assert(node->prev->next == node);
                if (node->next) Assert(node->next->prev == node);
                sum      += node->span;
                line_sum += node->line_span;
            }
            Assert(sum      == root->span);
            Assert(line_sum == root->line_span);
        }

        LineIndexNode *record = GetFirstRecord(root);
        ValidateTokenBlockChain(record);
    }
#endif

    return true;
}

function bool
ValidateLineIndexFull(Buffer *buffer)
{
    return ValidateLineIndexInternal(buffer->line_index_root, buffer);
}

function bool
ValidateLineIndexTreeIntegrity(LineIndexNode *root)
{
#if VALIDATE_LINE_INDEX_TREE_INTEGRITY_AGGRESSIVELY
    return ValidateLineIndexInternal(root, nullptr);
#else
    (void)root;
    return true;
#endif
}
