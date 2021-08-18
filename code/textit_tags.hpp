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
    Tag *parent;

    union
    {
        Tag *next_in_hash;
        Tag *next_free;
    };

    BufferID buffer;

    TokenKind related_token_kind;
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

struct TagParser
{
    Buffer *buffer;
    TokenIterator it;
    Token null_token;
    Token *t;
    bool pushed_text;
    uint8_t text_container_storage[512];
    StringContainer text_container;
    TokenFlags require_flags;
    TokenFlags reject_flags;
};

function void ParseTags(Buffer *buffer);

#endif /* TEXTIT_PARSER_HPP */