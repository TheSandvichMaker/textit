#ifndef TEXTIT_SHARED_HPP
#define TEXTIT_SHARED_HPP

static constexpr inline bool
IsPow2(size_t size)
{
    return (size != 0 && (size & (size - 1)) == 0);
}

// TODO: where tf do I put this
TEXTIT_INLINE Color
MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    return color;
}

#define COLOR_BLACK MakeColor(0, 0, 0)
#define COLOR_WHITE MakeColor(255, 255, 255)

template <typename T>
struct DeferDoodad
{
    T lambda;
    DeferDoodad(T lambda) : lambda(lambda) {}
    ~DeferDoodad() { lambda(); }
};

struct DeferDoodadHelp
{
    template <typename T>
    DeferDoodad<T> operator + (T t) { return t; }
};

#define defer const auto Paste(defer_, __LINE__) = DeferDoodadHelp() + [&]()

#define ArrayCount(x) (sizeof(x) / sizeof((x)[0]))

static inline void
ZeroSize(size_t size_init, void *data_init)
{
    size_t size = size_init;
    char *data = (char *)data_init;
    while (size--)
    {
        *data++ = 0;
    }
}

#define ZeroStruct(Struct) ZeroSize(sizeof(*(Struct)), Struct)
#define ZeroArray(Count, Data) ZeroSize(sizeof(*(Data))*(Count), Data)

static inline bool
MemoryIsEqual(size_t Count, void *AInit, void *BInit)
{
    char *A = (char *)AInit;
    char *B = (char *)BInit;

    bool Result = true;
    while (Count--)
    {
        if (*A++ != *B++)
        {
            Result = false;
            break;
        }
    }
    return Result;
}

#define StructsAreEqual(A, B) (Assert(sizeof(*(A)) == sizeof(*(B))), MemoryIsEqual(sizeof(*(A)), A, B))

static inline void
CopySize(size_t Size, const void *SourceInit, void *DestInit)
{
    const unsigned char *Source = (const unsigned char *)SourceInit;
    unsigned char *Dest = (unsigned char *)DestInit;
#if COMPILER_MSVC
    __movsb(Dest, Source, Size);
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("rep movsb" : "+c"(Size), "+S"(Source), "+D"(Dest): : "memory");
#else
    while (--Size)
    {
        *Dest++ = *Source++;
    }
#endif
}
#define CopyStruct(source, dest) CopySize(sizeof(*(source)), source, dest)
#define CopyArray(Count, Source, Dest) CopySize(sizeof(*(Source))*Count, Source, Dest)

struct HashResult
{
    union
    {
        __m128i m128i;
        uint64_t u64[2];
        uint32_t u32[4];
        uint16_t u16[8];
        uint8_t u8[16];
    };
};

static inline bool
operator == (HashResult a, HashResult b)
{
    return (a.u64[0] == b.u64[0] &&
            a.u64[1] == b.u64[1]);
}

static inline HashResult
HashData(HashResult seed, size_t len, const void *data_init)
{
    const char *data = (const char *)data_init;

    __m128i hash = seed.m128i;

    uint64_t len16 = len / 16;
    __m128i *at = (__m128i *)data;
    while (len16--)
    {
        hash = _mm_aesdec_si128(hash, _mm_loadu_si128(at));
        hash = _mm_aesdec_si128(hash, seed.m128i);
        hash = _mm_aesdec_si128(hash, seed.m128i);
        ++at;
    }

    char overhang[16] = {};
    for (size_t i = 0; i < len % 16; ++i)
    {
        overhang[i] = ((char *)at)[i];
    }
    hash = _mm_aesdec_si128(hash, _mm_loadu_si128((__m128i*)overhang));
    hash = _mm_aesdec_si128(hash, seed.m128i);
    hash = _mm_aesdec_si128(hash, seed.m128i);

    HashResult result;
    result.m128i = hash;
    return result;
}

static inline uint64_t
HashData(uint64_t seed, size_t len, const void *data)
{
    HashResult full_seed = {};
    full_seed.u64[0] = seed;

    HashResult full_result = HashData(full_seed, len, data);

    return ExtractU64(full_result.m128i, 0);
}

static inline HashResult
HashString(String string)
{
    HashResult seed = {};
    HashResult result = HashData(seed, string.size, string.data);
    return result;
}

static inline uint64_t
HashIntegers(uint64_t a, uint64_t b = 0)
{
    unsigned char seed[16] =
    {
        8,   45,  125, 36,
        205, 22,  36,  155,
        10,  127, 212, 213,
        197, 48,  36,  148,
    };
    __m128i hash = _mm_set_epi64x(b, a);
    hash = _mm_aesdec_si128(hash, _mm_loadu_si128((__m128i *)seed));
    hash = _mm_aesdec_si128(hash, _mm_loadu_si128((__m128i *)seed));
    return *(uint64_t *)&hash;
}

static inline uint64_t
HashCoordinate(V2i p)
{
    return HashIntegers(p.x, p.y);
}

template <typename T>
struct Array
{
    size_t capacity;
    size_t count;
    T *data;

    T &
    operator [](size_t i)
    {
        AssertSlow(i < count);
        return data[i];
    }
};

template <typename T>
static inline T *
begin(Array<T> &array)
{
    return array.data;
}

template <typename T>
static inline T *
end(Array<T> &array)
{
    return array.data + array.count;
}

template <typename T>
static inline void
Push(Array<T> *array_init, T item)
{
    Array<T> &array = *array_init;
    AssertSlow(array.count < array.capacity);
    array[array.count++] = item;
}

template <typename T>
static inline T
RemoveOrdered(Array<T> *array_init, size_t remove_index)
{
    Array<T> &array = *array_init;
    AssertSlow(remove_index < array.count);

    T result = array[remove_index];

    array.count -= 1;
    for (size_t i = remove_index; i < array.count; ++i)
    {
        array[i] = array[i + 1];
    }

    return result;
}

static inline int32_t
SafeTruncateI64(int64_t value)
{
    Assert((value >= INT32_MIN) &&
           (value <= INT32_MAX));
    return (int32_t)value;
}

#endif /* TEXTIT_SHARED_HPP */
