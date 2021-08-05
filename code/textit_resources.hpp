#ifndef TEXTIT_RESOURCES_HPP
#define TEXTIT_RESOURCES_HPP

struct ResourceID
{
    uint16_t index;
    uint16_t generation;

    operator bool(void)
    {
        return index != 0;
    }

    bool
    operator==(ResourceID other)
    {
        return ((index == other.index) &&
                (generation == other.generation));
    }

    bool
    operator!=(ResourceID other)
    {
        return ((index != other.index) ||
                (generation != other.generation));
    }

    static ResourceID
    Null(void)
    {
        ResourceID result = {};
        return result;
    }
};

typedef ResourceID BufferID;
typedef ResourceID ViewID;

#endif /* TEXTIT_RESOURCES_HPP */
