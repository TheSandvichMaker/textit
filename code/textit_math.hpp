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

#define MakeVectorInternal(type, ...) type { __VA_ARGS__ }

function V2 MakeV2(float s) { return MakeVectorInternal(V2, s, s); }
function V2 MakeV2(float x, float y) { return MakeVectorInternal(V2, x, y); }
function V3 MakeV3(float s) { return MakeVectorInternal(V3, s, s, s); }
function V3 MakeV3(V2 xy, float z) { return MakeVectorInternal(V3, xy.x, xy.y, z); }
function V3 MakeV3(float x, V2 yz) { return MakeVectorInternal(V3, x, yz.x, yz.y); }
function V3 MakeV3(float x, float y, float z) { return MakeVectorInternal(V3, x, y, z); }
function V4 MakeV4(float s) { return MakeVectorInternal(V4, s, s, s, s); }
function V4 MakeV4(V2 xy, float z, float w) { return MakeVectorInternal(V4, xy.x, xy.y, z, w); }
function V4 MakeV4(float x, float y, V2 zw) { return MakeVectorInternal(V4, x, y, zw.x, zw.y); }
function V4 MakeV4(V2 xy, V2 zw) { return MakeVectorInternal(V4, xy.x, xy.y, zw.x, zw.y); }
function V4 MakeV4(V3 xyz, float w) { return MakeVectorInternal(V4, xyz.x, xyz.y, xyz.z, w); }
function V4 MakeV4(float x, V3 yzw) { return MakeVectorInternal(V4, x, yzw.x, yzw.y, yzw.z); }
function V4 MakeV4(float x, float y, float z, float w) { return MakeVectorInternal(V4, x, y, z, w); }

function V3
MakeColorF(float x, float y, float z)
{
    V3 result;
    result.x = x*x;
    result.y = y*y;
    result.z = z*z;
    return result;
}

function V2i MakeV2i(int64_t s) { return MakeVectorInternal(V2i, s, s); }
function V2i MakeV2i(int64_t x, int64_t y) { return MakeVectorInternal(V2i, x, y); }
function V3i MakeV3i(int64_t s) { return MakeVectorInternal(V3i, s, s, s); }
function V3i MakeV3i(V2i xy, int64_t z) { return MakeVectorInternal(V3i, xy.x, xy.y, z); }
function V3i MakeV3i(int64_t x, V2i yz) { return MakeVectorInternal(V3i, x, yz.x, yz.y); }
function V3i MakeV3i(int64_t x, int64_t y, int64_t z) { return MakeVectorInternal(V3i, x, y, z); }
function V4i MakeV4i(int64_t s) { return MakeVectorInternal(V4i, s, s, s, s); }
function V4i MakeV4i(V2i xy, int64_t z, int64_t w) { return MakeVectorInternal(V4i, xy.x, xy.y, z, w); }
function V4i MakeV4i(int64_t x, int64_t y, V2i zw) { return MakeVectorInternal(V4i, x, y, zw.x, zw.y); }
function V4i MakeV4i(V2i xy, V2i zw) { return MakeVectorInternal(V4i, xy.x, xy.y, zw.x, zw.y); }
function V4i MakeV4i(V3i xyz, int64_t w) { return MakeVectorInternal(V4i, xyz.x, xyz.y, xyz.z, w); }
function V4i MakeV4i(int64_t x, V3i yzw) { return MakeVectorInternal(V4i, x, yzw.x, yzw.y, yzw.z); }
function V4i MakeV4i(int64_t x, int64_t y, int64_t z, int64_t w) { return MakeVectorInternal(V4i, x, y, z, w); }

#define IMPLEMENT_V2_VECTOR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        return result;                                       \
    }                                                        \
    function type &                                     \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V2_SCALAR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        return result;                                       \
    }                                                        \
    function type                                       \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        return result;                                       \
    }                                                        \
    function type &                                     \
    operator Paste(op, =) (type &a, scalar_type b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V3_VECTOR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        result.z = a.z op b.z;                               \
        return result;                                       \
    }                                                        \
    function type &                                     \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V3_SCALAR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        result.z = a.z op b;                                 \
        return result;                                       \
    }                                                        \
    function type                                       \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        result.z = a op b.z;                                 \
        return result;                                       \
    }                                                        \
    function type &                                     \
    operator Paste(op, =) (type &a, scalar_type b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V4_VECTOR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b.x;                               \
        result.y = a.y op b.y;                               \
        result.z = a.z op b.z;                               \
        result.w = a.w op b.w;                               \
        return result;                                       \
    }                                                        \
    function type &                                     \
    operator Paste(op, =) (type &a, const type &b)           \
    {                                                        \
        a = a op b;                                          \
        return a;                                            \
    }

#define IMPLEMENT_V4_SCALAR_OPERATORS(type, scalar_type, op) \
    function type                                       \
    operator op (const type &a, scalar_type b)               \
    {                                                        \
        type result;                                         \
        result.x = a.x op b;                                 \
        result.y = a.y op b;                                 \
        result.z = a.z op b;                                 \
        result.w = a.w op b;                                 \
        return result;                                       \
    }                                                        \
    function type                                       \
    operator op (scalar_type a, const type &b)               \
    {                                                        \
        type result;                                         \
        result.x = a op b.x;                                 \
        result.y = a op b.y;                                 \
        result.z = a op b.z;                                 \
        result.w = a op b.w;                                 \
        return result;                                       \
    }                                                        \
    function type &                                     \
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

function bool
AreEqual(V2i a, V2i b)
{
    return (a.x == b.x &&
            a.y == b.y);
}

function V2i
Perpendicular(V2i a)
{
    return MakeV2i(-a.y, a.x);
}

function float Square(float x) { return x*x; }
function float SquareRoot(float x) { return sqrtf(x); }
function V2 Square(V2 x) { return x*x; }
function V2 SquareRoot(V2 x) { return { SquareRoot(x.x), SquareRoot(x.y) }; }
function V3 Square(V3 x) { return x*x; }
function V3 SquareRoot(V3 x) { return { SquareRoot(x.x), SquareRoot(x.y), SquareRoot(x.z) }; }

function float Sin(float x) { return sinf(x); }
function float Cos(float x) { return cosf(x); }
function float Tan(float x) { return tanf(x); }
function float ASin(float x) { return asinf(x); }
function float ACos(float x) { return acosf(x); }
function float ATan(float x) { return atanf(x); }
function float ATan2(float y, float x) { return atan2f(y, x); }
function float SinH(float x) { return sinhf(x); }
function float CosH(float x) { return coshf(x); }
function float TanH(float x) { return tanhf(x); }
function float ASinH(float x) { return asinhf(x); }
function float ACosH(float x) { return acoshf(x); }
function float ATanH(float x) { return atanhf(x); }
function float Abs(float x) { return fabsf(x); }

function int64_t
Log10(int64_t x)
{
    int64_t result = 0;
    x /= 10;
    while (x > 0)
    {
        result += 1;
        x /= 10;
    }
    return result;
}

function int64_t
Abs(int64_t x)
{
    return (x < 0 ? -x : x);
}

function int64_t
DivFloor(int64_t a, int64_t b)
{
    Assert(b != 0);
    int64_t res = a / b;
    int64_t rem = a % b;
    int64_t corr = (rem != 0 && ((rem < 0) != (b < 0)));
    return res - corr;
}

function float
DegToRad(float deg)
{
    float rad = deg*(Pi32 / 180.0f);
    return rad;
}

function float
RadToDeg(float rad)
{
    float deg = rad*(180.0f / Pi32);
    return deg;
}

function uint32_t
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

function uint64_t
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

function float
Smoothstep(float x)
{
    float result = x*x*(3.0f - 2.0f*x);
    return result;
}

function float
Smootherstep(float x)
{
    float result = x*x*x*(x*(x*6.0f - 15.0f) + 10.0f);
    return result;
}

function float
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

function float
SafeRatioN(float numerator, float divisor, float n)
{
    float result = n;
    
    if (divisor != 0.0f)
    {
        result = numerator / divisor;
    }
    
    return result;
}

function float
SafeRatio0(float numerator, float divisor)
{
    float result = SafeRatioN(numerator, divisor, 0.0f);
    return result;
}

function float
SafeRatio1(float numerator, float divisor)
{
    float result = SafeRatioN(numerator, divisor, 1.0f);
    return result;
}

function float
SignOf(float a)
{
    return (a >= 0.0f ? 1.0f : -1.0f);
}

function int64_t
SignOf(int64_t a)
{
    return (a >= 0 ? 1 : -1);
}

function float
Min(float a, float b)
{
    return (a < b ? a : b);
}

function V2
Min(V2 a, V2 b)
{
    V2 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    return result;
}

function V3
Min(V3 a, V3 b)
{
    V3 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    return result;
}

function V4
Min(V4 a, V4 b)
{
    V4 result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    result.w = (a.w < b.w ? a.w : b.w);
    return result;
}

function float
Max(float a, float b)
{
    return (a > b ? a : b);
}

function V2
Max(V2 a, V2 b)
{
    V2 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    return result;
}

function V3
Max(V3 a, V3 b)
{
    V3 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    return result;
}

function V4
Max(V4 a, V4 b)
{
    V4 result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    result.w = (a.w > b.w ? a.w : b.w);
    return result;
}

function float
Clamp(float a, float min, float max)
{
    if (a < min) a = min;
    if (a > max) a = max;
    return a;
}

function V2
Clamp(V2 a, V2 min, V2 max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    return a;
}

function V3
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

function V4
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

function float
Clamp01(float a)
{
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return a;
}

function float
Clamp01MapToRange(float t, float min, float max)
{
    float result = Clamp01(MapToRange(t, min, max));
    return result;
}

function V2
Clamp01(V2 a)
{
    if (a.x < 0.0f) a.x = 0.0f;
    if (a.x > 1.0f) a.x = 1.0f;
    if (a.y < 0.0f) a.y = 0.0f;
    if (a.y > 1.0f) a.y = 1.0f;
    return a;
}

function V3
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

function V4
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

function float
NeighborhoodDistance(float a, float b, float period)
{
    float d0 = Abs(a - b);
    float d1 = Abs(a - period*SignOf(a) - b);
    float result = Min(d0, d1);
    return result;
}

function float Dot(V2 a, V2 b) { return a.x*b.x + a.y*b.y; }
function float Dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
function float Dot(V4 a, V4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
function float LengthSq(V2 a) { return a.x*a.x + a.y*a.y; }
function float LengthSq(V3 a) { return a.x*a.x + a.y*a.y + a.z*a.z; }
function float LengthSq(V4 a) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
function float Length(V2 a) { return SquareRoot(a.x*a.x + a.y*a.y); }
function float Length(V3 a) { return SquareRoot(a.x*a.x + a.y*a.y + a.z*a.z); }
function float Length(V4 a) { return SquareRoot(a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w); }

function V2
Normalize(V2 a)
{
    float length = (a.x*a.x + a.y*a.y);
    return a*(1.0f / length);
};

function V3
Normalize(V3 a)
{
    float length = (a.x*a.x + a.y*a.y + a.z*a.z);
    return a*(1.0f / length);
};

function V4
Normalize(V4 a)
{
    float length = (a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w);
    return a*(1.0f / length);
};

function float
Lerp(float a, float b, float t)
{
    return (1.0f - t)*a + t*b;
}

function V2
Lerp(V2 a, V2 b, float t)
{
    V2 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    return result;
}

function V2
Lerp(V2 a, V2 b, V2 t)
{
    V2 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    return result;
}

function V3
Lerp(V3 a, V3 b, float t)
{
    V3 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    result.z = (1.0f - t)*a.z + t*b.z;
    return result;
}

function V3
Lerp(V3 a, V3 b, V3 t)
{
    V3 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    result.z = (1.0f - t.z)*a.z + t.z*b.z;
    return result;
}

function V4
Lerp(V4 a, V4 b, float t)
{
    V4 result;
    result.x = (1.0f - t)*a.x + t*b.x;
    result.y = (1.0f - t)*a.y + t*b.y;
    result.z = (1.0f - t)*a.z + t*b.z;
    result.w = (1.0f - t)*a.w + t*b.w;
    return result;
}

function V4
Lerp(V4 a, V4 b, V4 t)
{
    V4 result;
    result.x = (1.0f - t.x)*a.x + t.x*b.x;
    result.y = (1.0f - t.y)*a.y + t.y*b.y;
    result.z = (1.0f - t.z)*a.z + t.z*b.z;
    result.w = (1.0f - t.w)*a.w + t.w*b.w;
    return result;
}

function V3
Cross(V3 a, V3 b)
{
    V3 result;
    result.x = a.y*b.z - a.z*b.y;
    result.y = a.z*b.x - a.x*b.z;
    result.z = a.x*b.y - a.y*b.x;
    return result;
}

function uint64_t LengthSq(V2i a) { return a.x*a.x + a.y*a.y; }
function uint64_t LengthSq(V3i a) { return a.x*a.x + a.y*a.y + a.z*a.z; }
function uint64_t LengthSq(V4i a) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
function float Length(V2i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y)); }
function float Length(V3i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y + a.z*a.z)); }
function float Length(V4i a) { return SquareRoot((float)(a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w)); }

function int64_t
Min(int64_t a, int64_t b)
{
    return (a < b ? a : b);
}

function size_t
Min(size_t a, size_t b)
{
    return (a < b ? a : b);
}

function V2i
Min(V2i a, V2i b)
{
    V2i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    return result;
}

function V3i
Min(V3i a, V3i b)
{
    V3i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    return result;
}

function V4i
Min(V4i a, V4i b)
{
    V4i result;
    result.x = (a.x < b.x ? a.x : b.x);
    result.y = (a.y < b.y ? a.y : b.y);
    result.z = (a.z < b.z ? a.z : b.z);
    result.w = (a.w < b.w ? a.w : b.w);
    return result;
}

function int64_t
Max(int64_t a, int64_t b)
{
    return (a > b ? a : b);
}

function V2i
Max(V2i a, V2i b)
{
    V2i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    return result;
}

function V3i
Max(V3i a, V3i b)
{
    V3i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    return result;
}

function V4i
Max(V4i a, V4i b)
{
    V4i result;
    result.x = (a.x > b.x ? a.x : b.x);
    result.y = (a.y > b.y ? a.y : b.y);
    result.z = (a.z > b.z ? a.z : b.z);
    result.w = (a.w > b.w ? a.w : b.w);
    return result;
}

function int64_t
Clamp(int64_t a, int64_t min, int64_t max)
{
    if (a < min) a = min;
    if (a > max) a = max;
    return a;
}

function V2i
Clamp(V2i a, V2i min, V2i max)
{
    if (a.x < min.x) a.x = min.x;
    if (a.x > max.x) a.x = max.x;
    if (a.y < min.y) a.y = min.y;
    if (a.y > max.y) a.y = max.y;
    return a;
}

function V3i
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

function V4i
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

function int64_t
Clamp01(int64_t a)
{
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    return a;
}

function V2i
Clamp01(V2i a)
{
    if (a.x < 0) a.x = 0;
    if (a.x > 1) a.x = 1;
    if (a.y < 0) a.y = 0;
    if (a.y > 1) a.y = 1;
    return a;
}

function V3i
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

function V4i
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

function int64_t
ManhattanDistance(V2i a, V2i b)
{
    int64_t result = Abs(a.x - b.x) + Abs(a.y - b.y);
    return result;
}

function int64_t
ManhattanDistance(V3i a, V3i b)
{
    int64_t result = Abs(a.x - b.x) + Abs(a.y - b.y) + Abs(a.z - b.z);
    return result;
}

function float
DiagonalDistance(V2i a, V2i b, float diagonal_cost = SquareRoot(2.0f))
{
    int64_t dx = Abs(a.x - b.x);
    int64_t dy = Abs(a.y - b.y);
    float result = (float)(dx + dy) + (diagonal_cost - 2.0f)*(float)Min(dx, dy);
    return result;
}

function float
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

function Rect2
MakeRect2MinMax(V2 min, V2 max)
{
    Rect2 result;
    result.min = min;
    result.max = max;
    return result;
}

function Rect2
MakeRect2MinDim(V2 min, V2 dim)
{
    Rect2 result;
    result.min = min;
    result.max = min + dim;
    return result;
}

function Rect2
MakeRect2CenterDim(V2 center, V2 dim)
{
    Rect2 result;
    result.min = center - 0.5f*dim;
    result.max = center + 0.5f*dim;
    return result;
}

function Rect2
MakeRect2CenterHalfDim(V2 center, V2 half_dim)
{
    Rect2 result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

function Rect2
AddDim(Rect2 a, V2 dim)
{
    Rect2 result;
    result.min = a.min - 0.5f*dim;
    result.max = a.max + 0.5f*dim;
    return result;
}

function Rect2
AddHalfDim(Rect2 a, V2 half_dim)
{
    Rect2 result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

function Rect2
Offset(Rect2 a, V2 offset)
{
    Rect2 result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

function bool
IsInRect(Rect2 a, V2 p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y);
    return result;
}

function Rect2
GrowToContain(Rect2 a, V2 p)
{
    Rect2 result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

function Rect2
Union(Rect2 a, Rect2 b)
{
    Rect2 result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

function Rect2
Intersect(Rect2 a, Rect2 b)
{
    Rect2 result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

function float
GetWidth(Rect2 a)
{
    return a.max.x - a.min.x;
}

function float
GetHeight(Rect2 a)
{
    return a.max.y - a.min.y;
}

//
// Rect2i
//

function Rect2i
MakeRect2iMinMax(int64_t min_x, int64_t min_y, int64_t max_x, int64_t max_y)
{
    Rect2i result;
    result.min.x = min_x;
    result.min.y = min_y;
    result.max.x = max_x;
    result.max.y = max_y;
    return result;
}

function Rect2i
MakeRect2iMinMax(V2i min, V2i max)
{
    Rect2i result;
    result.min = min;
    result.max = max;
    return result;
}

function Rect2i
MakeRect2iMinDim(int64_t min_x, int64_t min_y, int64_t dim_x, int64_t dim_y)
{
    Rect2i result;
    result.min.x = min_x;
    result.min.y = min_y;
    result.max.x = min_x + dim_x;
    result.max.y = min_y + dim_y;
    return result;
}

function Rect2i
MakeRect2iMinDim(V2i min, V2i dim)
{
    Rect2i result;
    result.min = min;
    result.max = min + dim;
    return result;
}

function Rect2i
MakeRect2iCenterHalfDim(V2i center, V2i half_dim)
{
    Rect2i result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

function void
SplitRect2iVertical(Rect2i rect, int64_t split_point, Rect2i *out_left, Rect2i *out_right)
{
    split_point = Clamp(rect.min.x + split_point, rect.min.x, rect.max.x);

    Rect2i left;
    left.min = rect.min;
    left.max = MakeV2i(split_point, rect.max.y);

    Rect2i right;
    right.min = MakeV2i(split_point, rect.min.y);
    right.max = rect.max;

    *out_left = left;
    *out_right = right;
}

function void
SplitRect2iHorizontal(Rect2i rect, int64_t split_point, Rect2i *out_bottom, Rect2i *out_top)
{
    split_point = Clamp(rect.min.y + split_point, rect.min.y, rect.max.y);

    Rect2i bottom;
    bottom.min = rect.min;
    bottom.max = MakeV2i(rect.max.x, split_point);

    Rect2i top;
    top.min = MakeV2i(rect.min.x, split_point);
    top.max = rect.max;

    *out_bottom = bottom;
    *out_top = top;
}

function int64_t
GetWidth(Rect2i a)
{
    return a.max.x - a.min.x;
}

function int64_t
GetHeight(Rect2i a)
{
    return a.max.y - a.min.y;
}

function Rect2i
AddHalfDim(Rect2i a, V2i half_dim)
{
    Rect2i result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

function Rect2i
Offset(Rect2i a, V2i offset)
{
    Rect2i result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

function Rect2i
Scale(Rect2i a, V2i mul)
{
    Rect2i result;
    result.min = a.min*mul;
    result.max = a.max*mul;
    return result;
}

function bool
IsInRect(Rect2i a, V2i p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y);
    return result;
}

function Rect2i
GrowToContain(Rect2i a, V2i p)
{
    Rect2i result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

function bool
RectanglesOverlap(Rect2i a, Rect2i b)
{
    bool result = !(b.max.x <= a.min.x ||
                    b.min.x >= a.max.x ||
                    b.max.y <= a.min.y ||
                    b.min.y >= a.max.y);
    return result;
}

function Rect2i
Union(Rect2i a, Rect2i b)
{
    Rect2i result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

function Rect2i
Intersect(Rect2i a, int64_t b_min_x, int64_t b_min_y, int64_t b_max_x, int64_t b_max_y)
{
    Rect2i result;
    result.min.x = Max(a.min.x, b_min_x);
    result.min.y = Max(a.min.y, b_min_y);
    result.max.x = Min(a.max.x, b_max_x);
    result.max.y = Min(a.max.y, b_max_y);
    return result;
}

function Rect2i
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

function Rect3
MakeRect3MinMax(V3 min, V3 max)
{
    Rect3 result;
    result.min = min;
    result.max = max;
    return result;
}

function Rect3
MakeRect3MinDim(V3 min, V3 dim)
{
    Rect3 result;
    result.min = min;
    result.max = min + dim;
    return result;
}

function Rect3
MakeRect3CenterDim(V3 center, V3 dim)
{
    Rect3 result;
    result.min = center - 0.5f*dim;
    result.max = center + 0.5f*dim;
    return result;
}

function Rect3
MakeRect3CenterHalfDim(V3 center, V3 half_dim)
{
    Rect3 result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

function float
GetWidth(Rect3 a)
{
    return a.max.x - a.min.x;
}

function float
GetHeight(Rect3 a)
{
    return a.max.y - a.min.y;
}

function float
GetDepth(Rect3 a)
{
    return a.max.z - a.min.z;
}

function Rect3
AddDim(Rect3 a, V3 dim)
{
    Rect3 result;
    result.min = a.min - 0.5f*dim;
    result.max = a.max + 0.5f*dim;
    return result;
}

function Rect3
AddHalfDim(Rect3 a, V3 half_dim)
{
    Rect3 result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

function Rect3
Offset(Rect3 a, V3 offset)
{
    Rect3 result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

function bool
IsInRect(Rect3 a, V3 p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y &&
                   p.y >= a.min.z && p.z < a.max.z);
    return result;
}

function Rect3
GrowToContain(Rect3 a, V3 p)
{
    Rect3 result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

function Rect3
Union(Rect3 a, Rect3 b)
{
    Rect3 result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

function Rect3
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

function Rect3i
MakeRect3iMinMax(V3i min, V3i max)
{
    Rect3i result;
    result.min = min;
    result.max = max;
    return result;
}

function Rect3i
MakeRect3iMinDim(V3i min, V3i dim)
{
    Rect3i result;
    result.min = min;
    result.max = min + dim;
    return result;
}

function Rect3i
MakeRect3iCenterHalfDim(V3i center, V3i half_dim)
{
    Rect3i result;
    result.min = center - half_dim;
    result.max = center + half_dim;
    return result;
}

function int64_t
GetWidth(Rect3i a)
{
    return a.max.x - a.min.x;
}

function int64_t
GetHeight(Rect3i a)
{
    return a.max.y - a.min.y;
}

function int64_t
GetDepth(Rect3i a)
{
    return a.max.z - a.min.z;
}

function Rect3i
AddHalfDim(Rect3i a, V3i half_dim)
{
    Rect3i result;
    result.min = a.min - half_dim;
    result.max = a.max + half_dim;
    return result;
}

function Rect3i
Offset(Rect3i a, V3i offset)
{
    Rect3i result;
    result.min = a.min + offset;
    result.max = a.max + offset;
    return result;
}

function bool
IsInRect(Rect3i a, V3i p)
{
    bool result = (p.x >= a.min.x && p.x < a.max.x &&
                   p.y >= a.min.y && p.y < a.max.y &&
                   p.z >= a.min.y && p.y < a.max.z);
    return result;
}

function Rect3i
GrowToContain(Rect3i a, V3i p)
{
    Rect3i result;
    result.min = Min(a.min, p);
    result.max = Max(a.max, p);
    return result;
}

function Rect3i
Union(Rect3i a, Rect3i b)
{
    Rect3i result;
    result.min = Min(a.min, b.min);
    result.max = Max(a.max, b.max);
    return result;
}

function Rect3i
Intersect(Rect3i a, Rect3i b)
{
    Rect3i result;
    result.min = Max(a.min, b.min);
    result.max = Min(a.max, b.max);
    return result;
}

#endif /* TEXTIT_MATH_HPP */
