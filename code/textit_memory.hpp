#ifndef TEXTIT_MEMORY_HPP
#define TEXTIT_MEMORY_HPP

#define DEFAULT_ARENA_CAPACITY Gigabytes(8)

function size_t
GetAlignOffset(Arena *arena, size_t Align)
{
    size_t Offset = (size_t)(arena->base + arena->used) & (Align - 1);
    if (Offset)
    {
        Offset = Align - Offset;
    }
    return Offset;
}

function char *
GetNextAllocationLocation(Arena *arena, size_t Align)
{
    size_t AlignOffset = GetAlignOffset(arena, Align);
    char* Result = arena->base + arena->used + AlignOffset;
    return Result;
}

function size_t
GetSizeRemaining(Arena *arena, size_t Align)
{
    size_t AlignOffset = GetAlignOffset(arena, Align);
    size_t Result = arena->capacity - (arena->used + AlignOffset);
    return Result;
}

function void
Clear(Arena *arena)
{
    Assert(arena->temp_count == 0);
    arena->used = 0;
    arena->temp_count = 0;
}

function void
Release(Arena *arena)
{
    Assert(arena->temp_count == 0);
    platform->DeallocateMemory(arena->base);
}

function void
ResetTo(Arena *arena, char *Target)
{
    Assert((Target >= arena->base) && (Target <= (arena->base + arena->used)));
    arena->used = (Target - arena->base);
}

function void
SetCapacity(Arena *arena, size_t capacity)
{
    Assert(!arena->capacity);
    Assert(!arena->base);
    arena->capacity = capacity;
}

function void
InitWithMemory(Arena *arena, size_t MemorySize, void *Memory)
{
    ZeroStruct(arena);
    arena->capacity = MemorySize;
    // NOTE: There's an assumption here that the memory passed in is valid, committed memory.
    //       If you want an Arena that exploits virtual memory to progressively commit, you
    //       shouldn't init it with any existing memory.
    arena->committed = MemorySize;
    arena->base = (char *)Memory;
}

function void
CheckArena(Arena* arena)
{
    Assert(arena->temp_count == 0);
}

#define PushStruct(arena, Type) \
    (Type *)PushSize_(arena, sizeof(Type), alignof(Type), true, LOCATION_STRING(#arena))
#define PushAlignedStruct(arena, Type, Align) \
    (Type *)PushSize_(arena, sizeof(Type), Align, true, LOCATION_STRING(#arena))
#define PushStructNoClear(arena, Type) \
    (Type *)PushSize_(arena, sizeof(Type), alignof(Type), false, LOCATION_STRING(#arena))
#define PushAlignedStructNoClear(arena, Type, Align) \
    (Type *)PushSize_(arena, sizeof(Type), Align, false, LOCATION_STRING(#arena))

#define PushArray(arena, Count, Type) \
    (Type *)PushSize_(arena, sizeof(Type)*(Count), alignof(Type), true, LOCATION_STRING(#arena))
#define PushAlignedArray(arena, Count, Type, Align) \
    (Type *)PushSize_(arena, sizeof(Type)*(Count), Align, true, LOCATION_STRING(#arena))
#define PushArrayNoClear(arena, Count, Type) \
    (Type *)PushSize_(arena, sizeof(Type)*(Count), alignof(Type), false, LOCATION_STRING(#arena))
#define PushAlignedArrayNoClear(arena, Count, Type, Align) \
    (Type *)PushSize_(arena, sizeof(Type)*(Count), Align, false, LOCATION_STRING(#arena))

#define PushSize(arena, size) \
    PushSize_(arena, size, 1, true, LOCATION_STRING(#arena))
#define PushSizeNoClear(arena, size) \
    PushSize_(arena, size, 1, false, LOCATION_STRING(#arena))
#define PushAlignedSize(arena, size, align) \
    PushSize_(arena, size, align, true, LOCATION_STRING(#arena))
#define PushAlignedSizeNoClear(arena, size, align) \
    PushSize_(arena, size, align, false, LOCATION_STRING(#arena))

function void *
PushSize_(Arena *arena, size_t size, size_t align, bool clear, const char *tag)
{
    if (!arena->capacity)
    {
        Assert(!arena->base);
        arena->capacity = DEFAULT_ARENA_CAPACITY;
    }

    if (!arena->base)
    {
        // NOTE: Let's align up to page size because that's the minimum allocation granularity anyway,
        //       and the code doing the commit down below assumes our capacity is page aligned.
        arena->capacity = AlignPow2(arena->capacity, platform->page_size);
        arena->base = (char *)platform->ReserveMemory(arena->capacity, PlatformMemFlag_NoLeakCheck, tag);
    }

    size_t align_offset = GetAlignOffset(arena, align);
    size_t aligned_size = size + align_offset;

    Assert((arena->used + aligned_size) <= arena->capacity);

    char *unaligned_base = arena->base + arena->used;

    if (arena->committed < (arena->used + aligned_size))
    {
        size_t commit_size = AlignPow2(aligned_size, platform->page_size);
        platform->CommitMemory(arena->base + arena->committed, commit_size);
        arena->committed += commit_size;
        Assert(arena->committed >= (arena->used + aligned_size));
    }

    void *result = unaligned_base + align_offset;
    arena->used += aligned_size;

    if (clear) {
        ZeroSize(size, result);
    }

    return result;
}

#define BootstrapPushStruct(Type, Member, ...)                                        \
    (Type *)BootstrapPushStruct_(sizeof(Type), alignof(Type), offsetof(Type, Member), \
                                 LOCATION_STRING("Bootstrap " #Type "::" #Member), ##__VA_ARGS__)
function void *
BootstrapPushStruct_(size_t Size, size_t Align, size_t arenaOffset, const char *Tag, size_t capacity = DEFAULT_ARENA_CAPACITY)
{
    Arena arena = {};
    SetCapacity(&arena, capacity);
    void *State = PushSize_(&arena, Size, Align, true, Tag);
    *(Arena *)((char *)State + arenaOffset) = arena;
    return State;
}

function Arena *
PushSubArena(Arena *arena, size_t capacity = SIZE_MAX)
{
    size_t max_capacity = GetSizeRemaining(arena, alignof(Arena));
    if (capacity > max_capacity) capacity = max_capacity;

    Arena *result = PushStruct(arena, Arena);
    result->capacity = capacity;
    result->base     = (char *)(result + 1);
    return result;
}

function char *
PushNullTerminatedString(Arena *arena, size_t size, char *data)
{
    char *result = PushArrayNoClear(arena, size + 1, char);

    CopyArray(size, data, result);
    result[size] = 0;

    return result;
}

function char *
PushNullTerminatedString(Arena *arena, String string)
{
    char *result = PushArrayNoClear(arena, string.size + 1, char);

    CopyArray(string.size, string.data, result);
    result[string.size] = 0;

    return result;
}

struct TemporaryMemory
{
    Arena *arena;
    size_t used;
};

function TemporaryMemory
BeginTemporaryMemory(Arena *arena)
{
    TemporaryMemory result = {};
    result.arena = arena;
    result.used  = arena->used;
    ++arena->temp_count;
    return result;
}

function void
EndTemporaryMemory(TemporaryMemory Temp)
{
    if (Temp.arena)
    {
        Assert(Temp.used <= Temp.arena->used);
        Temp.arena->used = Temp.used;
        --Temp.arena->temp_count;
    }
}

function void
CommitTemporaryMemory(TemporaryMemory *Temp)
{
    Arena *arena = Temp->arena;
    if (arena)
    {
        Assert(arena->temp_count > 0);
        --arena->temp_count;
        Temp->arena = NULL;
    }
}

struct ScopedMemory
{
    TemporaryMemory temp;
    ScopedMemory(Arena *arena) { temp = BeginTemporaryMemory(arena); }
    ScopedMemory(TemporaryMemory temp) : temp(temp) {}
    ~ScopedMemory() { EndTemporaryMemory(temp); }
    operator TemporaryMemory *() { return &temp; }
};

template <typename T>
function Array<T>
PushArrayContainer(Arena *arena, size_t capacity)
{
    Array<T> result = {};
    result.capacity = capacity;
    result.data = PushArray(arena, capacity, T);
    return result;
}

template <typename T, unsigned Capacity>
struct VirtualArray
{
    static constexpr unsigned capacity = Capacity;
    unsigned count;
    unsigned committed;
    T *data;

    T &
    operator [] (size_t index)
    {
        AssertSlow(index < count);
        return data[index];
    }

    T *
    End()
    {
        return data + count;
    }

    void
    Clear()
    {
        // TODO: Decommit behaviour
        count = 0;
    }

    void
    Release()
    {
        platform->DeallocateMemory(data);
        count     = 0;
        committed = 0;
        data      = nullptr;
    }

    bool Empty()
    {
        return count == 0;
    }

    unsigned
    EnsureSpace(unsigned push_count = 1)
    {
        unsigned new_count = count + push_count;
        if (new_count > capacity)
        {
            new_count = capacity;
        }
        unsigned diff = new_count - count;

        if (!data)
        {
            size_t to_reserve = AlignPow2(sizeof(T)*capacity, platform->allocation_granularity);
            data = (T *)platform->ReserveMemory(to_reserve, 0, LOCATION_STRING("Virtual Array"));
        }

        if (new_count > committed)
        {
            size_t to_commit = AlignPow2(sizeof(T)*diff, platform->page_size);
            size_t count_committed = to_commit / sizeof(T);

            void *result = platform->CommitMemory(data + committed, to_commit);
            committed += (unsigned)count_committed;

            Assert(result);
        }

        return diff;
    }

    T *
    Push(unsigned push_count = 1)
    {
        unsigned real_push_count = EnsureSpace(push_count);
        T *result = &data[count];
        count += real_push_count;
        return result;
    }

    T *
    Push(const T &item)
    {
        if (EnsureSpace(1) == 0)
        {
            INVALID_CODE_PATH;
        }
        T *result = &data[count++];
        *result = item;
        return result;
    }
};

#endif /* TEXTIT_MEMORY_HPP */
