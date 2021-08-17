#ifndef TEXTIT_PARSER_HPP
#define TEXTIT_PARSER_HPP

enum TagKind : uint8_t
{
    Tag_None,
    Tag_Declaration,
    Tag_Definition,
    Tag_COUNT,
};

typedef uint8_t TagSubKind;

struct Tag
{
    Tag *next;
    Tag *prev;

    union
    {
        Tag *next_in_hash;
        Tag *next_free;
    };

    BufferID buffer;

    TagKind kind;
    TagSubKind sub_kind;
    int16_t length;

    HashResult hash;
    int64_t pos;
};

struct Tags
{
    Tag sentinel;
};

function void ParseTags(Buffer *buffer);

#endif /* TEXTIT_PARSER_HPP */
