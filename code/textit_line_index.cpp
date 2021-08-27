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
    }
}

function void
FreeLineIndexNode(Buffer *buffer, LineIndexNode *node)
{
    Assert(node->kind == LineIndexNode_Record);
    FreeTokens(buffer, node);
    SllStackPush(buffer->first_free_line_index_node, node);
}

struct LineIndexLocator
{
    LineIndexNode *parent;
    int            child_index;
    int64_t        pos;
    int64_t        line;
};

template <bool by_line>
function void
LocateLineIndexNode(LineIndexNode    *node,      
                    int64_t           target,     
                    LineIndexLocator *locator,   
                    int64_t           offset      = 0, 
                    int64_t           line_offset = 0)
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
        locator->parent      = node;
        locator->child_index = next_index;
        locator->pos         = offset;
        locator->line        = line_offset;
        return;
    }

    LocateLineIndexNode<by_line>(child, target, locator, offset, line_offset);
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
    LineIndexLocator locator;
    LocateLineIndexNode<by_line>(buffer->line_index_root, target, &locator);

    LineIndexNode *node = locator.parent->children[locator.child_index];
    Assert(node->kind == LineIndexNode_Record);

    LineData *data = &node->data;

    ZeroStruct(out_info);
    out_info->line        = locator.line;
    out_info->range       = MakeRangeStartLength(locator.pos, node->span);
    out_info->newline_pos = locator.pos + data->newline_col;
    out_info->data        = data;
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

function LineIndexNode *
RemoveEntry(LineIndexNode *node, int entry_index)
{
    Assert(entry_index < node->entry_count);
    LineIndexNode *result = node->children[entry_index];

    if (result->prev) result->prev->next = result->next;
    if (result->next) result->next->prev = result->prev;

    for (LineIndexNode *parent = node; parent; parent = parent->parent)
    {
        parent->span      -= result->span;
        parent->line_span -= 1;
    }

    for (int i = entry_index; i < node->entry_count - 1; i += 1)
    {
        node->children[i] = node->children[i + 1];
    }
    node->entry_count -= 1;

#if TEXTIT_SLOW
    {
        LineIndexNode *root = result;
        for (; root->parent; root = root->parent);
        ValidateLineIndex(root, nullptr);
    }
#endif

    return result;
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
    Assert(result->kind == LineIndexNode_Leaf);
    return result;
}

function LineIndexNode *
InsertLine(Buffer *buffer, Range range, LineData *data)
{
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

    AssertSlow(ValidateLineIndex(buffer));

    return record;
}

//
// Line Index Iterator
//

function LineIndexIterator
IterateLineIndex(Buffer *buffer)
{
    LineIndexIterator result = {};
    result.leaf   = GetFirstLeafNode(buffer->line_index_root);
    result.record = result.leaf->children[0];
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
    result.leaf   = locator.parent;
    result.record = locator.parent->children[locator.child_index];
    result.index  = locator.child_index;
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
    result.leaf   = locator.parent;
    result.record = locator.parent->children[locator.child_index];
    result.index  = locator.child_index;
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
    it->range.start = it->range.end;

    it->index += 1;
    while (it->leaf && it->index >= it->leaf->entry_count)
    {
        it->leaf  = it->leaf->next;
        it->index = 0;
    }

    if (it->leaf)
    {
        it->record = it->leaf->children[it->index];

        it->range.end = it->range.start + it->record->span;
        it->line += 1;
    }
    else
    {
        it->record = nullptr;
    }
}

function void
Prev(LineIndexIterator *it)
{
    it->range.end = it->range.start;

    it->index -= 1;
    while (it->leaf && it->index < 0)
    {
        it->leaf = it->leaf->prev;
        if (it->leaf)
        {
            it->index = it->leaf->entry_count - 1;
        }
    }

    if (it->leaf)
    {
        it->record = it->leaf->children[it->index];

        it->range.start = it->range.end - it->record->span;
        it->line -= 1;
    }
    else
    {
        it->record = nullptr;
    }
}

function LineIndexNode *
RemoveCurrent(LineIndexIterator *it)
{
    LineIndexNode *result = RemoveEntry(it->leaf, it->index);
    if (it->index >= it->leaf->entry_count)
    {
        it->leaf  = it->leaf->next;
        it->index = 0;
    }
    it->record = it->leaf->children[it->index];

    it->range.end = it->range.start + it->record->span;
    return result;
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

//
// Token Iterator
//

function void
Rewind(TokenIterator *it, TokenLocator locator)
{
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
}

function TokenLocator
LocateTokenAtPos(LineInfo *info, int64_t pos)
{
    TokenLocator result = {};

    int64_t at_pos = info->range.start;
    for (TokenBlock *block = info->data->first_token_block;
         block;
         block = block->next)
    {
        for (int64_t index = 0; index < block->token_count; index += 1)
        {
            Token *token = &block->tokens[index];
            if (at_pos + token->length > pos)
            {
                result.block = block;
                result.index = index;
                result.pos   = at_pos;
                return result;
            }
            at_pos += token->length;
        }
    }

    return result;
}

function TokenIterator
IterateTokens(Buffer *buffer, int64_t pos = 0)
{
    TokenIterator result = {};

    LineInfo info;
    FindLineInfoByPos(buffer, pos, &info);

    TokenLocator locator = LocateTokenAtPos(&info, pos);
    Rewind(&result, locator);

    return result;
}

function TokenIterator
IterateLineTokens(Buffer *buffer, int64_t line)
{
    TokenIterator result = {};

    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    TokenLocator locator = LocateTokenAtPos(&info, info.range.start);
    Rewind(&result, locator);

    return result;
}

function TokenIterator
IterateLineTokens(LineInfo *info)
{
    TokenIterator result = {};

    TokenLocator locator = LocateTokenAtPos(info, info->range.start);
    Rewind(&result, locator);

    return result;
}

function bool
IsValid(TokenIterator *it)
{
    return it->block;
}

function TokenLocator
GetLocator(TokenIterator *it)
{
    TokenLocator locator = {};
    locator.block = it->block;
    locator.index = it->index;
    locator.pos   = it->token.pos;
    return locator;
}

function TokenLocator
LocateNext(TokenIterator *it, int offset = 1)
{
    if (!IsValid(it)) return {};

    TokenLocator result = {};

    TokenBlock *block  = it->block;
    int64_t     index  = it->index;
    int64_t     pos    = it->token.pos;
    int64_t     length = it->token.length;

    while (it->block && offset > 0)
    {
        pos += length;

        offset -= 1;
        index  += 1;
        while (block && index >= block->token_count)
        {
            block = block->next;
            index = 0;
        }

        if (block)
        {
            Token *t = &block->tokens[index];
            length = t->length;
        }
    }

    result.block = block;
    result.index = index;
    result.pos   = pos;

    return result;
}

function Token
PeekNext(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocateNext(it, offset);
    Token token = GetToken(locator);
    return token;
}

function Token
Next(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocateNext(it, offset);
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
    return it->token;
}

function TokenLocator
LocatePrev(TokenIterator *it, int offset = 1)
{
    if (!IsValid(it)) return {};

    TokenLocator result = {};

    TokenBlock *block  = it->block;
    int64_t     index  = it->index;
    int64_t     pos    = it->token.pos;
    int64_t     length = it->token.length;

    while (it->block && offset > 0)
    {
        pos -= length;

        offset -= 1;
        index  -= 1;
        while (block && index < 0)
        {
            block = block->next;
            index = block ? block->token_count - 1 : 0;
        }

        if (block)
        {
            Token *t = &block->tokens[index];
            length = t->length;
        }
    }

    result.block = block;
    result.index = index;
    result.pos   = pos;

    return result;
}

function Token
PeekPrev(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocatePrev(it, offset);
    Token token = GetToken(locator);
    return token;
}

function Token
Prev(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocatePrev(it, offset);
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
    return it->token;
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

    int64_t line_count = RangeSize(line_range);
    for (int64_t i = 0; i < line_count; i += 1)
    {
        LineIndexNode *node = RemoveCurrent(&it);
        FreeLineIndexNode(buffer, node);
    }

    AssertSlow(ValidateLineIndex(buffer));
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

    AssertSlow(ValidateLineIndex(buffer));
}

function bool
ValidateLineIndex(Buffer *buffer)
{
    return ValidateLineIndex(buffer->line_index_root, buffer);
}

function bool
ValidateLineIndex(LineIndexNode *root, Buffer *buffer)
{
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

            if (node->kind == LineIndexNode_Record)
            {
                LineData *data = &node->data;
                int64_t newline_size = node->span - data->newline_col;

                Assert(newline_size == 1 || newline_size == 2);

                if (buffer)
                {
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
        int64_t root_span      = root->span;
        int64_t root_line_span = root->line_span;
        int64_t sum            = 0;
        int64_t line_sum       = 0;
        for (LineIndexNode *node = GetFirstLeafNode(root);
             node;
             node = node->next)
        {
            Assert(node->kind      == LineIndexNode_Leaf);
            Assert(node->line_span == node->entry_count);
            if (node->prev) Assert(node->prev->next == node);
            if (node->next) Assert(node->next->prev == node);
            sum      += node->span;
            line_sum += node->line_span;
        }
        Assert(root_span      == sum);
        Assert(root_line_span == line_sum);
    }

    return true;
}
