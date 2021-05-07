#ifndef TEXTIT_MATH_HPP
#define TEXTIT_MATH_HPP

// TODO: Eliminate this include for cool programmer brownie points
#include <math.h>

// Cool constants

#define Pi32 3.14159265f
#define Tau32 6.28318531f
#define Pi64 3.14159265358979324
#define Tau64 6.28318530717958648
#define RcpPi32 0.318309886f
#define RcpTau32 0.159154943f
#define RcpPi64 0.318309886183790672
#define RcpTau64 0.159154943091895336

// Constructor functions

#if COMPILER_MSVC
#define MakeVectorInternal(type, ...) type { __VA_ARGS__ }
#else
#define MakeVectorInternal(type, ...) (type) { __VA_ARGS__ }
#endif

TEXTIT_INLINE V2 MakeV2(float s) { return MakeVectorInternal(V2, s, s); }
TEXTIT_INLINE V2 MakeV2(float x, float y) { return MakeVectorInternal(V2, x, y); }
TEXTIT_INLINE V3 MakeV3(float s) { return MakeVectorInternal(V3, s, s, s); }
TEXTIT_INLINE V3 MakeV3(V2 xy, float z) { return MakeVectorInternal(V3, xy[0], xy[1], z); }
TEXTIT_INLINE V3 MakeV3(float x, V2 yz) { return MakeVectorInternal(V3, x, yz[0], yz[1]); }
TEXTIT_INLINE V3 MakeV3(float x, float y, float z) { return MakeVectorInternal(V3, x, y, z); }
TEXTIT_INLINE V4 MakeV4(float s) { return MakeVectorInternal(V4, s, s, s, s); }
TEXTIT_INLINE V4 MakeV4(V2 xy, float z, float w) { return MakeVectorInternal(V4, xy[0], xy[1], z, w); }
TEXTIT_INLINE V4 MakeV4(float x, float y, V2 zw) { return MakeVectorInternal(V4, x, y, zw[0], zw[1]); }
TEXTIT_INLINE V4 MakeV4(V2 xy, V2 zw) { return MakeVectorInternal(V4, xy[0], xy[1], zw[0], zw[1]); }
TEXTIT_INLINE V4 MakeV4(V3 xyz, float w) { return MakeVectorInternal(V4, xyz[0], xyz[1], xyz[2], w); }
TEXTIT_INLINE V4 MakeV4(float x, V3 yzw) { return MakeVectorInternal(V4, x, yzw[0], yzw[1], yzw[2]); }
TEXTIT_INLINE V4 MakeV4(float x, float y, float z, float w) { return MakeVectorInternal(V4, x, y, z, w); }

TEXTIT_INLINE V3
MakeColorF(float x, float y, float z)
{
    V3 result;
    result.x = x*x;
    result.y = y*y;
    result.z = z*z;
    return result;
}

TEXTIT_INLINE V2i MakeV2i(int64_t s) { return MakeVectorInternal(V2i, s, s); }
TEXTIT_INLINE V2i MakeV2i(int64_t x, int64_t y) { return MakeVectorInternal(V2i, x, y); }
TEXTIT_INLINE V3i MakeV3i(int64_t s) { return MakeVectorInternal(V3i, s, s, s); }
TEXTIT_INLINE V3i MakeV3i(V2i xy, int64_t z) { return MakeVectorInternal(V3i, xy[0], xy[1], z); }
TEXTIT_INLINE V3i MakeV3i(int64_t x, V2i yz) { return MakeVectorInternal(V3i, x, yz[0], yz[1]); }
TEXTIT_INLINE V3i MakeV3i(int64_t x, int64_t y, int64_t z) { return MakeVectorInternal(V3i, x, y, z); }
TEXTIT_INLINE V4i MakeV4i(int64_t s) { return MakeVectorInternal(V4i, s, s, s, s); }
TEXTIT_INLINE V4i MakeV4i(V2i xy, int64_t z, int64_t w) { return MakeVectorInternal(V4i, xy[0], xy[1], z, w); }
TEXTIT_INLINE V4i MakeV4i(int64_t x, int64_t y, V2i zw) { return MakeVectorInternal(V4i, x, y, zw[0], zw[1]); }
TEXTIT_INLINE V4i MakeV4i(V2i xy, V2i zw) { return MakeVectorInternal(V4i, xy[0], xy[1], zw[0], zw[1]); }
TEXTIT_INLINE V4i MakeV4i(V3i xyz, int64_t w) { return MakeVectorInternal(V4i, xyz[0], xyz[1], xyz[2], w); }
TEXTIT_INLINE V4i MakeV4i(int64_t x, V3i yzw) { return MakeVectorInternal(V4i, x, yzw[0], yzw[1], yzw[2]); }
TEXTIT_INLINE V4i MakeV4i(int64_t x, int64_t y, int64_t z, int64_t w) { return MakeVectorInternal(V4i, x, y, z, w); }

#if COMPILER_MSVC

#define IMPLEMENT_V2_VECTOR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V2_SCALAR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type                                     \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, scalar_type b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V3_VECTOR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        result.z = a.z op b.z;                               \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V3_SCALAR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        result.z = a.z op b;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type                                     \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        result.z = a op b.z;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, scalar_type b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V4_VECTOR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        result.z = a.z op b.z;                               \
        result.w = a.w op b.w;                               \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V4_SCALAR_OPERATORS(type, scalar_type, op) \
    TEXTIT_INLINE type                                     \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        result.z = a.z op b;                                 \
        result.w = a.w op b;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type                                     \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        result.z = a op b.z;                                 \
        result.w = a op b.w;                                 \
        return result;                                       \
    }                                                        \
    TEXTIT_INLINE type &                                   \
    operator Paste(op, =) (type &a, scalar_type b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

IMPLEMENT_V2_VECTOR_OPERATORS(V2, float, +)
IMPLEMENT_V2_VECTOR_OPERATORS(V2, float, -)
IMPLEMENT_V2_SCALAR_OPERATORS(V2, float, *)
IMPLEMENT_V2_SCALAR_OPERATORS(V2, float, /)
IMPLEMENT_V2_VECTOR_OPERATORS(V2, float, *)
IMPLEMENT_V2_VECTOR_OPERATORS(V2, float, /)

IMPLEMENT_V3_VECTOR_OPERATORS(V3, float, +)
IMPLEMENT_V3_VECTOR_OPERATORS(V3, float, -)
IMPLEMENT_V3_SCALAR_OPERATORS(V3, float, *)
IMPLEMENT_V3_SCALAR_OPERATORS(V3, float, /)
IMPLEMENT_V3_VECTOR_OPERATORS(V3, float, *)
IMPLEMENT_V3_VECTOR_OPERATORS(V3, float, /)

IMPLEMENT_V4_VECTOR_OPERATORS(V4, float, +)
IMPLEMENT_V4_VECTOR_OPERATORS(V4, float, -)
IMPLEMENT_V4_SCALAR_OPERATORS(V4, float, *)
IMPLEMENT_V4_SCALAR_OPERATORS(V4, float, /)
IMPLEMENT_V4_VECTOR_OPERATORS(V4, float, *)
IMPLEMENT_V4_VECTOR_OPERATORS(V4, float, /)

IMPLEMENT_V2_VECTOR_OPERATORS(V2i, int64_t, +)
IMPLEMENT_V2_VECTOR_OPERATORS(V2i, int64_t, -)
IMPLEMENT_V2_SCALAR_OPERATORS(V2i, int64_t, *)
IMPLEMENT_V2_SCALAR_OPERATORS(V2i, int64_t, /)
IMPLEMENT_V2_VECTOR_OPERATORS(V2i, int64_t, *)
IMPLEMENT_V2_VECTOR_OPERATORS(V2i, int64_t, /)

IMPLEMENT_V3_VECTOR_OPERATORS(V3i, int64_t, +)
IMPLEMENT_V3_VECTOR_OPERATORS(V3i, int64_t, -)
IMPLEMENT_V3_SCALAR_OPERATORS(V3i, int64_t, *)
IMPLEMENT_V3_SCALAR_OPERATORS(V3i, int64_t, /)
IMPLEMENT_V3_VECTOR_OPERATORS(V3i, int64_t, *)
IMPLEMENT_V3_VECTOR_OPERATORS(V3i, int64_t, /)

IMPLEMENT_V4_VECTOR_OPERATORS(V4i, int64_t, +)
IMPLEMENT_V4_VECTOR_OPERATORS(V4i, int64_t, -)
IMPLEMENT_V4_SCALAR_OPERATORS(V4i, int64_t, *)
IMPLEMENT_V4_SCALAR_OPERATORS(V4i, int64_t, /)
IMPLEMENT_V4_VECTOR_OPERATORS(V4i, int64_t, *)
IMPLEMENT_V4_VECTOR_OPERATORS(V4i, int64_t, /)

#endif

TEXTIT_INLINE bool
AreEqual(V2i a, V2i b)
{
    return (a.x == b.x &&
            a.y == b.y);
}

TEXTIT_INLINE V2i
Perpendicular(V2i a)
{
    return MakeV2i(-a.y, a.x);
}

TEXTIT_INLINE float Square(float x) { return x*x; }
TEXTIT_INLINE float SquareRoot(float x) { return sqrtf(x); }
TEXTIT_INLINE V2 Square(V2 x) { return x*x; }
TEXTIT_INLINE V2 SquareRoot(V2 x) { return { SquareRoot(x.x), SquareRoot(x.y) }; }
TEXTIT_INLINE V3 Square(V3 x) { return x*x; }
TEXTIT_INLINE V3 SquareRoot(V3 x) { return { SquareRoot(x.x), SquareRoot(x.y), SquareRoot(x.z) }; }

TEXTIT_INLINE float Sin(float x) { return sinf(x); }
TEXTIT_INLINE float Cos(float x) { return cosf(x); }
TEXTIT_INLINE float Tan(float x) { return tanf(x); }
TEXTIT_INLINE float ASin(float x) { return asinf(x); }
TEXTIT_INLINE float ACos(float x) { return acosf(x); }
TEXTIT_INLINE float ATan(float x) { return atanf(x); }
TEXTIT_INLINE float ATan2(float y, float x) { return atan2f(y, x); }
TEXTIT_INLINE float SinH(float x) { return sinhf(x); }
TEXTIT_INLINE float CosH(float x) { return coshf(x); }
TEXTIT_INLINE float TanH(float x) { return tanhf(x); }
TEXTIT_INLINE float ASinH(float x) { return asinhf(x); }
TEXTIT_INLINE float ACosH(float x) { return acoshf(x); }
TEXTIT_INLINE float ATanH(float x) { return atanhf(x); }
TEXTIT_INLINE float Abs(float x) { return fabsf(x); }

TEXTIT_INLINE int64_t
Abs(int64_t x)
{
    return (x < 0 ? -x : x);
}

TEXTIT_INLINE int64_t
DivFloor(int64_t a, int64_t b)
{
    Assert(b != 0);
    int64_t res = a / b;
    int64_t rem = a % b;
    int64_t corr = (rem != 0 && ((rem < 0) != (b < 0)));
    return res - corr;
}

TEXTIT_INLINE float
DegToRad(float deg)
{
    float rad = deg*(Pi32 / 180.0f);
    return rad;
}

TEXTIT_INLINE float
RadToDeg(float rad)
{
    float deg = rad*(180.0f / Pi32);
    return deg;
}

TEXTIT_INLINE uint32_t
RoundNextPow2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

TEXTIT_INLINE uint64_t
RoundNextPow2(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

TEXTIT_INLINE float
Smoothstep(float x)
{
    float result = x*x*(3.0f - 2.0f*x);
    return result;
}

TEXTIT_INLINE float
Smootherstep(float x)
{
    float result = x*x*x*(x*(x*6.0f - 15.0f) + 10.0f);
    return result;
}

TEXTIT_INLINE float
MapToRange(float t, float min, float max)
{
    float result = 0.0f;
    float range = max - min;
    if (range != 0.0f)
    {
        result = (t - min) / range;
    }
    return result;
}

TEXTIT_INLINE float
SafeRatioN(float numerator, float divisor, float n)
{
    float result = n;
    
    if (divisor != 0.0f)
    {
        result = numerator / divisor;
    }
    
    return result;
}

TEXTIT_INLINE float
SafeRatio0(float numerator, float divisor)
{
    float result = SafeRatioN(numerator, divisor, 0.0f);
    return result;
}

TEXTIT_INLINE float
SafeRatio1(float numerator, float divisor)
{
    float result = SafeRatioN(numerator, divisor, 1.0f);
    return result;
}

TEXTIT_INLINE float
SignOf(float a)
{
    return (a >= 0.0f ? 1.0f : 0.0f);
}

TEXTIT_INLINE float
Min(float a, float b)
{
    return (a < b ? a : b);
}

TEXTIT_INLINE V2
Min(V2 a, V2 b)
{
    V2 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    return result;
}

TEXTIT_INLINE V3
Min(V3 a, V3 b)
{
    V3 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    return result;
}

TEXTIT_INLINE V4
Min(V4 a, V4 b)
{
    V4 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    result.w = (a.w < b.w ? a.w : b.w);
    return result;
}

TEXTIT_INLINE float
Max(float a, float b)
{
    return (a > b ? a : b);
}

TEXTIT_INLINE V2
Max(V2 a, V2 b)
{
    V2 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    return result;
}

TEXTIT_INLINE V3
Max(V3 a, V3 b)
{
    V3 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    return result;
}

TEXTIT_INLINE V4
Max(V4 a, V4 b)
{
    V4 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    result.w = (a.w > b.w ? a.w : b.w);
    return result;
}

TEXTIT_INLINE float
Clamp(float a, float min, float max)
{
    if (a < min) a = min;
    if (a > max) a = max;
    return a;
}

TEXTIT_INLINE V2
Clamp(V2 a, V2 min, V2 max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    return a;
}

TEXTIT_INLINE V3
Clamp(V3 a, V3 min, V3 max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    if (a.z < min.z) a.z = min.z;
    if (a.z > max.z) a.z = max.z;
    return a;
}

TEXTIT_INLINE V4
Clamp(V4 a, V4 min, V4 max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    if (a.z < min.z) a.z = min.z;
    if (a.z > max.z) a.z = max.z;
    if (a.w < min.w) a.w = min.w;
    if (a.w > max.w) a.w = max.w;
    return a;
}

TEXTIT_INLINE float
Clamp01(float a)
{
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return a;
}

TEXTIT_INLINE float
Clamp01MapToRange(float t, float min, float max)
{
    float result = Clamp01(MapToRange(t, min, max));
    return result;
}

TEXTIT_INLINE V2
Clamp01(V2 a)
{
    if (a.x < 0.0f) a.x = 0.0f;
    if (a.x > 1.0f) a.x = 1.0f;
    if (a.y < 0.0f) a.y = 0.0f;
    if (a.y > 1.0f) a.y = 1.0f;
    return a;
}

TEXTIT_INLINE V3
Clamp01(V3 a)
{
    if (a.x < 0.0f) a.x = 0.0f;
    if (a.x > 1.0f) a.x = 1.0f;
    if (a.y < 0.0f) a.y = 0.0f;
    if (a.y > 1.0f) a.y = 1.0f;
    if (a.z < 0.0f) a.z = 0.0f;
    if (a.z > 1.0f) a.z = 1.0f;
    return a;
}

TEXTIT_INLINE V4
Clamp01(V4 a)
{
    if (a.x < 0.0f) a.x = 0.0f;
    if (a.x > 1.0f) a.x = 1.0f;
    if (a.y < 0.0f) a.y = 0.0f;
    if (a.y > 1.0f) a.y = 1.0f;
    if (a.z < 0.0f) a.z = 0.0f;
    if (a.z > 1.0f) a.z = 1.0f;
    if (a.w < 0.0f) a.w = 0.0f;
    if (a.w > 1.0f) a.w = 1.0f;
    return a;
}

TEXTIT_INLINE float
NeighborhoodDistance(float a, float b, float period)
{
    float d0 = Abs(a - b);
    float d1 = Abs(a - period*SignOf(a) - b);
    float result = Min(d0, d1);
    return result;
}

TEXTIT_INLINE float Dot(V2 a, V2 b) { return a.x*b.x + a.y*b.y; }
TEXTIT_INLINE float Dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
TEXTIT_INLINE float Dot(V4 a, V4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
TEXTIT_INLINE float LengthSq(V2 a) { return a.x*a.x + a.y*a.y; }
TEXTIT_INLINE float LengthSq(V3 a) { return a.x*a.x + a.y*a.y + a.z*a.z; }
TEXTIT_INLINE float LengthSq(V4 a) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
TEXTIT_INLINE float Length(V2 a) { return SquareRoot(a.x*a.x + a.y*a.y); }
TEXTIT_INLINE float Length(V3 a) { return SquareRoot(a.x*a.x + a.y*a.y + a.z*a.z); }
TEXTIT_INLINE float Length(V4 a) { return SquareRoot(a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w); }

TEXTIT_INLINE V2
Normalize(V2 a)
{
    float length = (a.x*a.x + a.y*a.y);
    return a*(1.0f / length);
};

TEXTIT_INLINE V3
Normalize(V3 a)
{
    float length = (a.x*a.x + a.y*a.y + a.z*a.z);
    return a*(1.0f / length);
};

TEXTIT_INLINE V4
Normalize(V4 a)
{
    float length = (a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w);
    return a*(1.0f / length);
};

TEXTIT_INLINE float
Lerp(float a, float b, float t)
{
    return (1.0f - t)*a + t*b;
}

TEXTIT_INLINE V2
Lerp(V2 a, V2 b, float t)
{
    V2 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    return result;
}

TEXTIT_INLINE V2
Lerp(V2 a, V2 b, V2 t)
{
    V2 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    return result;
}

TEXTIT_INLINE V3
Lerp(V3 a, V3 b, float t)
{
    V3 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    result.z = (1.0f - t)*a.z + t*b.z;
    return result;
}

TEXTIT_INLINE V3
Lerp(V3 a, V3 b, V3 t)
{
    V3 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    result.z = (1.0f - t.z)*a.z + t.z*b.z;
    return result;
}

TEXTIT_INLINE V4
Lerp(V4 a, V4 b, float t)
{
    V4 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    result.z = (1.0f - t)*a.z + t*b.z;
    result.w = (1.0f - t)*a.w + t*b.w;
    return result;
}

TEXTIT_INLINE V4
Lerp(V4 a, V4 b, V4 t)
{
    V4 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    result.z = (1.0f - t.z)*a.z + t.z*b.z;
    result.w = (1.0f - t.w)*a.w + t.w*b.w;
    return result;
}

TEXTIT_INLINE V3
Cross(V3 a, V3 b)
{
    V3 result;
    result.x = a.y*b.z - a.z*b.y;
    result.y = a.z*b.x - a.x*b.z;
    result.z = a.x*b.y - a.y*b.x;
    return result;
}

TEXTIT_INLINE uint64_t LengthSq(V2i a) { return a.x*a.x + a.y*a.y; }
TEXTIT_INLINE uint64_t LengthSq(V3i a) { return a.x*a.x + a.y*a.y + a.z*a.z; }
TEXTIT_INLINE uint64_t LengthSq(V4i a) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
TEXTIT_INLINE float Length(V2i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y)); }
TEXTIT_INLINE float Length(V3i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y + a.z*a.z)); }
TEXTIT_INLINE float Length(V4i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w)); }

TEXTIT_INLINE int64_t
Min(int64_t a, int64_t b)
{
    return (a < b ? a : b);
}

TEXTIT_INLINE V2i
Min(V2i a, V2i b)
{
    V2i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    return result;
}

TEXTIT_INLINE V3i
Min(V3i a, V3i b)
{
    V3i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    return result;
}

TEXTIT_INLINE V4i
Min(V4i a, V4i b)
{
    V4i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    result.w = (a.w < b.w ? a.w : b.w);
    return result;
}

TEXTIT_INLINE int64_t
Max(int64_t a, int64_t b)
{
    return (a > b ? a : b);
}

TEXTIT_INLINE V2i
Max(V2i a, V2i b)
{
    V2i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    return result;
}

TEXTIT_INLINE V3i
Max(V3i a, V3i b)
{
    V3i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    return result;
}

TEXTIT_INLINE V4i
Max(V4i a, V4i b)
{
    V4i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    result.w = (a.w > b.w ? a.w : b.w);
    return result;
}

TEXTIT_INLINE int64_t
Clamp(int64_t a, int64_t min, int64_t max)
{
    if (a < min) a = min;
    if (a > max) a = max;
    return a;
}

TEXTIT_INLINE V2i
Clamp(V2i a, V2i min, V2i max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    return a;
}

TEXTIT_INLINE V3i
Clamp(V3i a, V3i min, V3i max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    if (a.z < min.z) a.z = min.z;
    if (a.z > max.z) a.z = max.z;
    return a;
}

TEXTIT_INLINE V4i
Clamp(V4i a, V4i min, V4i max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    if (a.z < min.z) a.z = min.z;
    if (a.z > max.z) a.z = max.z;
    if (a.w < min.w) a.w = min.w;
    if (a.w > max.w) a.w = max.w;
    return a;
}

TEXTIT_INLINE int64_t
Clamp01(int64_t a)
{
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    return a;
}

TEXTIT_INLINE V2i
Clamp01(V2i a)
{
    if (a.x < 0) a.x = 0;
    if (a.x > 1) a.x = 1;
    if (a.y < 0) a.y = 0;
    if (a.y > 1) a.y = 1;
    return a;
}

TEXTIT_INLINE V3i
Clamp01(V3i a)
{
    if (a.x < 0) a.x = 0;
    if (a.x > 1) a.x = 1;
    if (a.y < 0) a.y = 0;
    if (a.y > 1) a.y = 1;
    if (a.z < 0) a.z = 0;
    if (a.z > 1) a.z = 1;
    return a;
}

TEXTIT_INLINE V4i
Clamp01(V4i a)
{
    if (a.x < 0) a.x = 0;
    if (a.x > 1) a.x = 1;
    if (a.y < 0) a.y = 0;
    if (a.y > 1) a.y = 1;
    if (a.z < 0) a.z = 0;
    if (a.z > 1) a.z = 1;
    if (a.w < 0) a.w = 0;
    if (a.w > 1) a.w = 1;
    return a;
}

TEXTIT_INLINE int64_t
ManhattanDistance(V2i a, V2i b)
{
    int64_t result = Abs(a.x - b.x) + Abs(a.y - b.y);
    return result;
}

TEXTIT_INLINE int64_t
ManhattanDistance(V3i a, V3i b)
{
    int64_t result = Abs(a.x - b.x) + Abs(a.y - b.y) + Abs(a.z - b.z);
    return result;
}

TEXTIT_INLINE float
DiagonalDistance(V2i a, V2i b, float diagonal_cost = SquareRoot(2.0f))
{
    int64_t dx = Abs(a.x - b.x);
    int64_t dy = Abs(a.y - b.y);
    float result = (float)(dx + dy) + (diagonal_cost - 2.0f)*(float)Min(dx, dy);
    return result;
}

TEXTIT_INLINE float
DiagonalDistance(V3i a, V3i b, float diagonal_cost = SquareRoot(2.0f))
{
    int64_t dx = Abs(a.x - b.x);
    int64_t dy = Abs(a.y - b.y);
    int64_t dz = Abs(a.z - b.z);
    float result = (float)(dx + dy + dz) + (diagonal_cost - 2.0f)*(float)Min(Min(dx, dy), dz);
    return result;
}

//
// Rect2
//

TEXTIT_INLINE Rect2
MakeRect2MinMax(V2 min, V2 max)
{
    Rect2 result;
    result.min = min;
    result.max = max;
    return result;
}

TEXTIT_INLINE Rect2
MakeRect2MinDim(V2 min, V2 dim)
{
    Rect2 result;
    result.min = min;
    result.max = min + dim;
    return result;
}

TEXTIT_INLINE Rect2
MakeRect2CenterDim(V2 center, V2 dim)
{
    Rect2 result;
    result.min = center - 0.5f*dim;
    result.max = center + 0.5f*dim;
    return result;
}

TEXTIT_INLINE Rect2
MakeRect2CenterHalfDim(V2 center, V2 half_dim)
{
    Rect2 result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

TEXTIT_INLINE Rect2
AddDim(Rect2 a, V2 dim)
{
    Rect2 result;
    result.min = a.min - 0.5f*dim;
    result.max = a.max + 0.5f*dim;
    return result;
}

TEXTIT_INLINE Rect2
AddHalfDim(Rect2 a, V2 half_dim)
{
    Rect2 result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

TEXTIT_INLINE Rect2
Offset(Rect2 a, V2 offset)
{
    Rect2 result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

TEXTIT_INLINE bool
IsInRect(Rect2 a, V2 p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y);
    return result;
}

TEXTIT_INLINE Rect2
GrowToContain(Rect2 a, V2 p)
{
    Rect2 result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

TEXTIT_INLINE Rect2
Union(Rect2 a, Rect2 b)
{
    Rect2 result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

TEXTIT_INLINE Rect2
Intersect(Rect2 a, Rect2 b)
{
    Rect2 result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

TEXTIT_INLINE float
GetWidth(Rect2 a)
{
    return a.max.x - a.min.x;
}

TEXTIT_INLINE float
GetHeight(Rect2 a)
{
    return a.max.y - a.min.y;
}

//
// Rect2i
//

TEXTIT_INLINE Rect2i
MakeRect2iMinMax(V2i min, V2i max)
{
    Rect2i result;
    result.min = min;
    result.max = max;
    return result;
}

TEXTIT_INLINE Rect2i
MakeRect2iMinDim(int64_t min_x, int64_t min_y, int64_t dim_x, int64_t dim_y)
{
    Rect2i result;
    result.min.x = min_x;
    result.min.y = min_y;
    result.max.x = min_x + dim_x;
    result.max.y = min_y + dim_y;
    return result;
}

TEXTIT_INLINE Rect2i
MakeRect2iMinDim(V2i min, V2i dim)
{
    Rect2i result;
    result.min = min;
    result.max = min + dim;
    return result;
}

TEXTIT_INLINE Rect2i
MakeRect2iCenterHalfDim(V2i center, V2i half_dim)
{
    Rect2i result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

TEXTIT_INLINE void
SplitRect2iHorizontal(Rect2i rect, int64_t split_point, Rect2i *out_left, Rect2i *out_right)
{
    split_point = Clamp(split_point, rect.min.x, rect.max.x);

    Rect2i left;
    left.min = rect.min;
    left.max = MakeV2i(split_point, rect.max.y);

    Rect2i right;
    right.min = MakeV2i(split_point, rect.min.x);
    right.max = rect.max;

    *out_left = left;
    *out_right = right;
}

TEXTIT_INLINE int64_t
GetWidth(Rect2i a)
{
    return a.max.x - a.min.x;
}

TEXTIT_INLINE int64_t
GetHeight(Rect2i a)
{
    return a.max.y - a.min.y;
}

TEXTIT_INLINE Rect2i
AddHalfDim(Rect2i a, V2i half_dim)
{
    Rect2i result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

TEXTIT_INLINE Rect2i
Offset(Rect2i a, V2i offset)
{
    Rect2i result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

TEXTIT_INLINE bool
IsInRect(Rect2i a, V2i p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y);
    return result;
}

TEXTIT_INLINE Rect2i
GrowToContain(Rect2i a, V2i p)
{
    Rect2i result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

TEXTIT_INLINE bool
RectanglesOverlap(Rect2i a, Rect2i b)
{
    bool result = !(b.max.x <= a.min.x ||
                    b.min.x >= a.max.x ||
                    b.max.y <= a.min.y ||
                    b.min.y >= a.max.y);
    return result;
}

TEXTIT_INLINE Rect2i
Union(Rect2i a, Rect2i b)
{
    Rect2i result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

TEXTIT_INLINE Rect2i
Intersect(Rect2i a, int64_t b_min_x, int64_t b_min_y, int64_t b_max_x, int64_t b_max_y)
{
    Rect2i result;
    result.min.x = Max(a.min.x, b_min_x);
    result.min.y = Max(a.min.y, b_min_y);
    result.max.x = Min(a.max.x, b_max_x);
    result.max.y = Min(a.max.y, b_max_y);
    return result;
}

TEXTIT_INLINE Rect2i
Intersect(Rect2i a, Rect2i b)
{
    Rect2i result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

//
// Rect3
//

TEXTIT_INLINE Rect3
MakeRect3MinMax(V3 min, V3 max)
{
    Rect3 result;
    result.min = min;
    result.max = max;
    return result;
}

TEXTIT_INLINE Rect3
MakeRect3MinDim(V3 min, V3 dim)
{
    Rect3 result;
    result.min = min;
    result.max = min + dim;
    return result;
}

TEXTIT_INLINE Rect3
MakeRect3CenterDim(V3 center, V3 dim)
{
    Rect3 result;
    result.min = center - 0.5f*dim;
    result.max = center + 0.5f*dim;
    return result;
}

TEXTIT_INLINE Rect3
MakeRect3CenterHalfDim(V3 center, V3 half_dim)
{
    Rect3 result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

TEXTIT_INLINE float
GetWidth(Rect3 a)
{
    return a.max.x - a.min.x;
}

TEXTIT_INLINE float
GetHeight(Rect3 a)
{
    return a.max.y - a.min.y;
}

TEXTIT_INLINE float
GetDepth(Rect3 a)
{
    return a.max.z - a.min.z;
}

TEXTIT_INLINE Rect3
AddDim(Rect3 a, V3 dim)
{
    Rect3 result;
    result.min = a.min - 0.5f*dim;
    result.max = a.max + 0.5f*dim;
    return result;
}

TEXTIT_INLINE Rect3
AddHalfDim(Rect3 a, V3 half_dim)
{
    Rect3 result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

TEXTIT_INLINE Rect3
Offset(Rect3 a, V3 offset)
{
    Rect3 result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

TEXTIT_INLINE bool
IsInRect(Rect3 a, V3 p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y &&
                   p.y >= a.min.z && p.z < a.max.z);
    return result;
}

TEXTIT_INLINE Rect3
GrowToContain(Rect3 a, V3 p)
{
    Rect3 result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

TEXTIT_INLINE Rect3
Union(Rect3 a, Rect3 b)
{
    Rect3 result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

TEXTIT_INLINE Rect3
Intersect(Rect3 a, Rect3 b)
{
    Rect3 result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

//
// Rect3i
//

TEXTIT_INLINE Rect3i
MakeRect3iMinMax(V3i min, V3i max)
{
    Rect3i result;
    result.min = min;
    result.max = max;
    return result;
}

TEXTIT_INLINE Rect3i
MakeRect3iMinDim(V3i min, V3i dim)
{
    Rect3i result;
    result.min = min;
    result.max = min + dim;
    return result;
}

TEXTIT_INLINE Rect3i
MakeRect3iCenterHalfDim(V3i center, V3i half_dim)
{
    Rect3i result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

TEXTIT_INLINE int64_t
GetWidth(Rect3i a)
{
    return a.max.x - a.min.x;
}

TEXTIT_INLINE int64_t
GetHeight(Rect3i a)
{
    return a.max.y - a.min.y;
}

TEXTIT_INLINE int64_t
GetDepth(Rect3i a)
{
    return a.max.z - a.min.z;
}

TEXTIT_INLINE Rect3i
AddHalfDim(Rect3i a, V3i half_dim)
{
    Rect3i result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

TEXTIT_INLINE Rect3i
Offset(Rect3i a, V3i offset)
{
    Rect3i result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

TEXTIT_INLINE bool
IsInRect(Rect3i a, V3i p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y &&
                   p.z >= a.min.y && p.y < a.max.z);
    return result;
}

TEXTIT_INLINE Rect3i
GrowToContain(Rect3i a, V3i p)
{
    Rect3i result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

TEXTIT_INLINE Rect3i
Union(Rect3i a, Rect3i b)
{
    Rect3i result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

TEXTIT_INLINE Rect3i
Intersect(Rect3i a, Rect3i b)
{
    Rect3i result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

#endif /* TEXTIT_MATH_HPP */
