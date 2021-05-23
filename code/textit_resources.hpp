#ifndef TEXTIT_RESOURCES_HPP
#define TEXTIT_RESOURCES_HPP

template <typename Tag>
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

using BufferID = ResourceID<struct BufferIDTag>;
using ViewID   = ResourceID<struct ViewIDTag>;

#endif /* TEXTIT_RESOURCES_HPP */
