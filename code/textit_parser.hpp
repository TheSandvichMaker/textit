#ifndef TEXTIT_PARSER_HPP
#define TEXTIT_PARSER_HPP

enum TagKind : uint8_t
{
    Tag_None,
    Tag_Declaration,
    Tag_Definition,
    Tag_COUNT,
};

struct Tag
{
    Tag *next;
    Tag *prev;
    Tag *next_in_hash;

    BufferID buffer;

    TagKind kind;
    uint8_t sub_kind;
    int16_t length;

    HashResult hash;
    int64_t pos;
};

struct Tags
{
    Tag sentinel;
};

#endif /* TEXTIT_PARSER_HPP */
