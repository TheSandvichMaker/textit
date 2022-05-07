#ifndef TEXTIT_SORT_HPP
#define TEXTIT_SORT_HPP

struct SortKey
{
    uint32_t key;
    uint32_t index;
};

template <typename T>
using SortComparator = bool (*)(const T &a, const T &b);

// general purpose sort 
template <typename T> function void Sort(size_t count, T *data, SortComparator<T> comparator);

// merge sort
template <typename T> function void MergeSortInternal(size_t count, T *a, T *b, SortComparator<T> comparator);
template <typename T> function void MergeSort(size_t count, T *data, SortComparator<T> comparator);

// radix sort
function void RadixSort(size_t count, uint32_t *data, uint32_t *temp);
function void RadixSort(size_t count, SortKey *data, SortKey *temp);

#endif /* TEXTIT_SORT_HPP */
