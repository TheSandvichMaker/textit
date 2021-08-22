// TODO: These nodes are supposed to store relative positions / lines, friendo

function LineIndexNode *
FindLineIndexNodeByPos(LineIndexNode *node, int64_t min_pos)
{
    if (node->kind == LineIndexNode_Leaf)
    {
        return node;
    }

    int next_index = 0;
    for (int i = 1; i < node->child_count; i += 1)
    {
        LineIndexKey key = node->keys[i];
        if (min_pos >= key.pos)
        {
            next_index = i;
        }
        else
        {
            break;
        }
    }

    return FindLineIndexNodeByPos(node->children[next_index], min_pos);
}

function LineData *
InsertIntoLineIndex(LineIndexNode *root, int64_t line, int64_t pos)
{
    LineIndexNode *node = FindLineIndexNodeByPos(root, pos);
    Assert(node->kind == LineIndexNode_Leaf);

    if (node->child_count == ArrayCount(node->records))
    {
        // split
    }
    else
    {
        // figure out insert index

        int insert_index = 0;
        for (int i = 1; i < node->child_count; i += 1)
        {
            LineIndexKey key = node->keys[i];
            if (min_pos >= key.pos)
            {
                insert_index = i;
            }
            else
            {
                break;
            }
        }

        // shift entries

        for (int i = ArrayCount(node->records) - 1; i > insert_index; i -= 1)
        {
            node->keys   [i - 1] = node->keys[i];
            node->records[i - 1] = node->records[i];
        }

        // insert

        LineIndexKey key;
        key.pos  = pos;
        key.line = line;

        node->keys   [insert_index] = key;
        node->records[insert_index] = AllocateLineData();
    }
}
