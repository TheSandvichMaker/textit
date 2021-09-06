#ifndef TEXTIT_PARSER_HPP
#define TEXTIT_PARSER_HPP

enum TagKind : uint8_t
{
    Tag_None,
    Tag_Declaration,
    Tag_Definition,
    Tag_CommentAnnotation,
    Tag_COUNT,
};

typedef uint8_t TagSubKind;

struct Tag
{
    Tag *next;
    Tag *prev;
    Tag *parent;

    Tag *next_in_hash;

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

function void ParseTags(Buffer *buffer);
function void FreeAllTags(Buffer *buffer);
function Tag *PushTagsWithName(Arena *arena, Project *project, String name);

struct TagParser
{
    Buffer *buffer;

    TokenIterator it;

    bool            pushed_text;
    uint8_t         text_container_storage[512];
    StringContainer text_container;

    TokenFlags require_flags;
    TokenFlags reject_flags;

    int parse_index;
};

function void InitializeTagParser(TagParser *parser, Buffer *buffer);
function TokenLocator GetLocator(TagParser *parser);
function void Rewind(TagParser *parser, TokenLocator locator);
function bool TokensLeft(TagParser *parser);
function bool AcceptToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, Token *t);
function void Advance(TagParser *parser);
function void SetFlags(TagParser *parser, TokenFlags require_flags, TokenFlags reject_flags);
function String GetTokenText(TagParser *parser);
function Token PeekToken(TagParser *parser);
function Token MatchToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {});
function Token MatchToken(TagParser *parser, TokenKind kind, String match_text = {});
function Token ConsumeToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {});
function Token ConsumeToken(TagParser *parser, TokenKind kind, String match_text = {});
function bool ConsumeBalancedPair(TagParser *parser, TokenKind left_kind);

#endif /* TEXTIT_PARSER_HPP */
