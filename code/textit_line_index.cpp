// TODO: These nodes are supposed to store relative positions / lines, friendo

function LineIndexNode *
AllocateLineIndexNode(Arena *arena, LineIndexNodeKind kind)
{
    LineIndexNode *result = PushStruct(arena, LineIndexNode);
    result->kind = kind;
    return result;
}

struct LineIndexLocator
{
    LineIndexNode *parent;
    int            child_index;
    int64_t        pos;
    int64_t        line;
};

function void
LocateLineIndexNodeByPos(LineIndexNode *node, int64_t pos, LineIndexLocator *locator, int64_t offset = 0, int64_t line_offset = 0)
{
    Assert(node->kind != LineIndexNode_Record);

    int next_index = 0;
    for (; next_index < node->entry_count; next_index += 1)
    {
        LineIndexNode *child = node->children[next_index];

        int64_t delta = (pos - offset) - child->span;
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

    LocateLineIndexNodeByPos(node->children[next_index], pos, locator, offset, line_offset);
}

function void
LocateLineIndexNodeByLine(LineIndexNode *node, int64_t line, LineIndexLocator *locator, int64_t offset = 0, int64_t line_offset = 0)
{
    Assert(node->kind != LineIndexNode_Record);

    int next_index = 0;
    for (; next_index < node->entry_count; next_index += 1)
    {
        LineIndexNode *child = node->children[next_index];

        int64_t delta = (line - line_offset) - child->line_span;
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

    LocateLineIndexNodeByLine(node->children[next_index], line, locator, offset, line_offset);
}

function void
FindLineInfoByPos(LineIndex *index, int64_t pos, LineInfo *out_info)
{
    LineIndexLocator locator;
    LocateLineIndexNodeByPos(index->root, pos, &locator);

    LineIndexNode *node = locator.parent->children[locator.child_index];

    Assert(node->kind == LineIndexNode_Record);

    LineData *data = &node->data;

    ZeroStruct(out_info);
    out_info->line        = locator.line;
    out_info->range       = MakeRangeStartLength(locator.pos, node->span);
    out_info->token_count = data->token_count;
    out_info->token_index = data->token_index;
}

function void
FindLineInfoByLine(LineIndex *index, int64_t line, LineInfo *out_info)
{
    LineIndexLocator locator;
    LocateLineIndexNodeByLine(index->root, line, &locator);

    LineIndexNode *node = locator.parent->children[locator.child_index];

    Assert(node->kind == LineIndexNode_Record);

    LineData *data = &node->data;

    ZeroStruct(out_info);
    out_info->line        = locator.line;
    out_info->range       = MakeRangeStartLength(locator.pos, node->span);
    out_info->token_count = data->token_count;
    out_info->token_index = data->token_index;
}

#if 0
function void *
RemoveEntry(LineIndexNode *node, int entry_index)
{
    Assert(entry_index < node->entry_count);
    void *result = (void *)node->children[entry_index];

    for (int i = entry_index; i < node->entry_count - 1; i += 1)
    {
        node->keys    [i] = node->keys    [i + 1];
        node->children[i] = node->children[i + 1];
    }
    node->entry_count -= 1;

    return result;
}
#endif

function LineIndexNode *
InsertEntry(Arena          *arena,
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

        // insert

        node->children[insert_index] = record;
        node->entry_count += 1;

        if (node->entry_count > 2*LINE_INDEX_ORDER)
        {
            int8_t  left_count = LINE_INDEX_ORDER;
            int8_t right_count = LINE_INDEX_ORDER + 1;

            LineIndexNode *l1 = node;
            LineIndexNode *l2 = AllocateLineIndexNode(arena, l1->kind);

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
                l2->span      += l2->children[i]->span;
                l2->line_span += l2->children[i]->line_span;
            }

            result   = l2;
            l1->next = l2;
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
        if (LineIndexNode *split = InsertEntry(arena, child, pos, record, offset))
        {
            // shift

            for (int i = node->entry_count; i > insert_index; i -= 1)
            {
                node->children[i] = node->children[i - 1];
            }

            // insert

            node->children[insert_index + 1] = split;
            node->entry_count += 1;

            if (node->entry_count > 2*LINE_INDEX_ORDER)
            {
                int8_t  left_count = LINE_INDEX_ORDER;
                int8_t right_count = LINE_INDEX_ORDER + 1;

                LineIndexNode *l1 = node;
                LineIndexNode *l2 = AllocateLineIndexNode(arena, l1->kind);

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
                    l2->span      += l2->children[i]->span;
                    l2->line_span += l2->children[i]->line_span;
                }

                result = l2;
            }
        }
    }
    return result;
}

function LineData *
InsertLine(LineIndex *index, Range range)
{
    Assert(index->arena);

    if (!index->root)
    {
        index->root = AllocateLineIndexNode(index->arena, LineIndexNode_Leaf);
    }

    LineIndexNode *record = AllocateLineIndexNode(index->arena, LineIndexNode_Record);
    record->span      = RangeSize(range);
    record->line_span = 1;

    if (LineIndexNode *split_node = InsertEntry(index->arena, index->root, range.start, record))
    {
        LineIndexNode *new_root = AllocateLineIndexNode(index->arena, LineIndexNode_Internal);
        new_root->entry_count = 2;
        new_root->children[0] = index->root;
        new_root->children[1] = split_node;
        new_root->span += new_root->children[0]->span;
        new_root->span += new_root->children[1]->span;
        index->root = new_root;
    }

    return &record->data;
}

function LineIndexNode *
GetFirstLeafNode(LineIndex *index)
{
    LineIndexNode *result = index->root;
    while (result->kind != LineIndexNode_Leaf)
    {
        Assert(result->entry_count > 0);
        result = result->children[0];
    }
    return result;
}

function LineIndexIterator
IterateLineIndex(LineIndex *index)
{
    LineIndexIterator result = {};
    result.leaf   = GetFirstLeafNode(index);
    result.record = result.leaf->children[0];
    result.range  = MakeRangeStartLength(0, result.record->span);
    result.line   = 0;
    return result;
}

function LineIndexIterator
IterateLineIndexFromLine(LineIndex *index, int64_t line)
{
    LineIndexLocator locator;
    LocateLineIndexNodeByLine(index->root, line, &locator);

    LineIndexIterator result = {};
    result.leaf   = locator.parent;
    result.record = locator.parent->children[locator.child_index];
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
    it->range.start += it->record->span;

    if (it->index >= it->leaf->entry_count)
    {
        it->leaf  = it->leaf->next;
        it->index = 0;
    }
    it->index += 1;
    it->record = it->leaf->children[it->index];

    it->range.end = it->range.start + it->record->span;
    it->line += 1;
}
