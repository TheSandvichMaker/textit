template <typename T>
function void
MergeSortInternal(uint32_t count, T *a, T *b, bool (*comparator)(T *a, T *b))
{
    if (count <= 1)
    {
        // Nothing to do
    }
    else
    {
        uint32_t count_l = count / 2;
        uint32_t count_r = count - count_l;
        
        MergeSortInternal(count_l, b, a, comparator);
        MergeSortInternal(count_r, b + count_l, a + count_l, comparator);
        
        T *middle = b + count_l;
        T *end    = b + count;
        
        T *l = b;
        T *r = b + count_l;
        
        T *out = a;
        for (size_t i = 0; i < count; ++i)
        {
            if ((l < middle) &&
                ((r >= end) || comparator(l, r)))
            {
                *out++ = *l++;
            }
            else
            {
                *out++ = *r++;
            }
        }
    }
}

template <typename T>
function void
MergeSort(uint32_t count, T *a, bool (*comparator)(T *a, T *b))
{
    ScopedMemory temp;
    T *b = PushArrayNoClear(temp, count, T);

    CopyArray(count, a, b);
    MergeSortInternal(count, a, b, comparator);
}

function void
RadixSort(uint32_t count, uint32_t *data, uint32_t *temp)
{
    uint32_t *source = data;
    uint32_t *dest   = temp;

    for (int byte_index = 0; byte_index < 4; ++byte_index)
    {
        uint32_t offsets[256] = {};

        // NOTE: First pass - count how many of each key
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i] >> 8*byte_index) & 0xFF;
            offsets[radix_piece] += 1;
        }

        // NOTE: Change counts to offsets
        uint32_t total = 0;
        for (size_t i = 0; i < ArrayCount(offsets); ++i)
        {
            uint32_t piece_count = offsets[i];
            offsets[i] = total;
            total += piece_count;
        }

        // NOTE: Second pass - place elements into dest in order
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i] >> 8*byte_index) & 0xFF;
            dest[offsets[radix_piece]++] = source[i];
        }

        Swap(dest, source);
    }
}

function void
RadixSort(uint32_t count, SortKey *data, SortKey *temp)
{
    SortKey *source = data;
    SortKey *dest   = temp;

    for (int byte_index = 0; byte_index < 4; ++byte_index)
    {
        uint32_t offsets[256] = {};

        // NOTE: First pass - count how many of each key
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i].key >> 8*byte_index) & 0xFF;
            offsets[radix_piece] += 1;
        }

        // NOTE: Change counts to offsets
        uint32_t total = 0;
        for (size_t i = 0; i < ArrayCount(offsets); ++i)
        {
            uint32_t piece_count = offsets[i];
            offsets[i] = total;
            total += piece_count;
        }

        // NOTE: Second pass - place elements into dest in order
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i].key >> 8*byte_index) & 0xFF;
            dest[offsets[radix_piece]++] = source[i];
        }

        Swap(dest, source);
    }
}
