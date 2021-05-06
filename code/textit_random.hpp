#ifndef TEXTIT_RANDOM_HPP
#define TEXTIT_RANDOM_HPP

struct RandomSeries
{
    uint64_t state;
};

static inline RandomSeries
MakeRandomSeries(uint64_t seed)
{
    RandomSeries result = {};
    result.state = seed;
    return result;
}

static inline uint32_t
NextInSeries(RandomSeries* series)
{
    uint64_t const multiplier = 0x7FACC0F7A00541BDull;
    series->state = series->state*multiplier + multiplier;
    uint32_t result = RotateRight((uint32_t)((series->state^(series->state >> 18)) >> 27), (uint32_t)(series->state >> 59));
    return result;
}

static inline float
RandomUnilateral(RandomSeries* series)
{
    // NOTE: Stolen from rnd.h, courtesy of Jonatan Hedborg
    uint32_t exponent = 127;
    uint32_t mantissa = NextInSeries(series) >> 9;
    uint32_t bits = (exponent << 23) | mantissa;
    float result = *(float*)&bits - 1.0f;
    return result;
}

static inline uint32_t
RandomChoice(RandomSeries* series, uint32_t range)
{
    uint32_t result = NextInSeries(series) % range;
    return result;
}

static inline uint32_t
DiceRoll(RandomSeries* series, uint32_t sides)
{
    uint32_t result = 1 + RandomChoice(series, sides);
    return result;
}

static inline int32_t
RandomRange(RandomSeries* series, int32_t min, int32_t max)
{
    if (max < min)
    {
        max = min;
    }
    int32_t result = min + (int32_t)(NextInSeries(series) % (max - min + 1));
    return result;
}

static inline V2i
RandomInRect(RandomSeries *series, Rect2i rect)
{
    V2i result;
    result.x = RandomRange(series, (int32_t)rect.min.x, (int32_t)rect.max.x);
    result.y = RandomRange(series, (int32_t)rect.min.y, (int32_t)rect.max.y);
    return result;
}

#endif /* TEXTIT_RANDOM_HPP */
