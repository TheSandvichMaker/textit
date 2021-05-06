#ifndef TEXTIT_MATH_TYPES_HPP
#define TEXTIT_MATH_TYPES_HPP

#if COMPILER_MSVC
struct V2
{
    union
    {
        struct
        {
            float x, y;
        };

        float e[2];
    };
    
    float &
    operator[](size_t index)
    {
        AssertSlow(index < 2);
        return e[index];
    }
};

struct V3
{
    union
    {
        struct
        {
            float x, y, z;
        };

        float e[3];
    };

    float &
    operator[](size_t index)
    {
        AssertSlow(index < 3);
        return e[index];
    }
};

struct V4
{
    union
    {
        struct
        {
            float x, y, z, w;
        };

        float e[4];
    };

    float &
    operator[](size_t index)
    {
        AssertSlow(index < 4);
        return e[index];
    }
};

struct V2i
{
    union
    {
        struct
        {
            int32_t x, y;
        };

        int32_t e[2];
    };

    int32_t &
    operator[](size_t index)
    {
        AssertSlow(index < 2);
        return e[index];
    }
};

struct V3i
{
    union
    {
        struct
        {
            int32_t x, y, z;
        };

        int32_t e[3];
    };

    int32_t &
    operator[](size_t index)
    {
        AssertSlow(index < 3);
        return e[index];
    }
};

struct V4i
{
    union
    {
        struct
        {
            int32_t x, y, z, w;
        };

        int32_t e[4];
    };

    int32_t &
    operator[](size_t index)
    {
        AssertSlow(index < 4);
        return e[index];
    }
};
#else
typedef float V2 __attribute__((ext_vector_type(2)));
typedef float V3 __attribute__((ext_vector_type(3)));
typedef float V4 __attribute__((ext_vector_type(4)));

typedef int32_t V2i __attribute__((ext_vector_type(2)));
typedef int32_t V3i __attribute__((ext_vector_type(3)));
typedef int32_t V4i __attribute__((ext_vector_type(4)));
#endif

struct Rect2
{
    V2 min, max;
};

struct Rect3
{
    V3 min, max;
};

struct Rect2i
{
    V2i min, max;
};

struct Rect3i
{
    V3i min, max;
};

#endif /* TEXTIT_MATH_TYPES_HPP */
