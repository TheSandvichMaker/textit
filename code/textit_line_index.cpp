// TODO: These nodes are supposed to store relative positions / lines, friendo

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
InsertLine(Buffer *buffer, Range range, LineData *data)
{
    PlatformHighResTime start = platform->GetTime();

    if (!buffer->line_index_root)
    {
        buffer->line_index_root = AllocateLineIndexNode(buffer, LineIndexNode_Leaf);
    }

    LineIndexNode *record = AllocateLineIndexNode(buffer, LineIndexNode_Record);
    record->span      = RangeSize(range);
    record->line_span = 1;
    CopyStruct(data, &record->data);

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
