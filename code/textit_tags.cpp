function Tag *
AllocateTag()
{
    if (!editor->first_free_tag)
    {
        editor->first_free_tag = PushStructNoClear(&editor->transient_arena, Tag);
        editor->first_free_tag->next_free = nullptr;
    }
    Tag *result = editor->first_free_tag;
    editor->first_free_tag = result->next_free;

    ZeroStruct(result);

    return result;
}

function void
FreeTag(Tag *tag)
{
    tag->next = editor->first_free_tag;
    editor->first_free_tag = tag;
}

function Tag **
GetTagSlot(Project *project, Tag *tag)
{
    Tag **result = &project->tag_table[tag->hash.u32[0] % ArrayCount(project->tag_table)];
    while (*result && *result != tag)
    {
        result = &(*result)->next_in_hash;
    }
    return result;
}

function Tag *
AddTagInternal(Buffer *buffer, String name)
{
    Tags *tags = buffer->tags;
    Project *project = buffer->project;

    Tag *result = AllocateTag();

    HashResult hash = HashString(name);
    result->hash = hash;

    Tag **slot = &project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
    result->next_in_hash = *slot;
    *slot = result;

    DllInsertBack(&tags->sentinel, result);

    result->buffer = buffer->id;

    return result;
}

function Tag *
AddTag(Buffer *buffer, Token *t)
{
    ScopedMemory temp;
    String name = PushTokenString(temp, buffer, t);
    Tag *tag = AddTagInternal(buffer, name);
    tag->related_token_kind = t->kind;
    tag->pos                = t->pos;
    tag->length             = t->length;
    return tag;
}

function void
FreeAllTags(Buffer *buffer)
{
    Project *project = buffer->project;

    Tags *tags = buffer->tags;
    while (DllIsntEmpty(&tags->sentinel))
    {
        Tag *tag = tags->sentinel.next;

        Tag **slot = GetTagSlot(project, tag);
        Assert(*slot == tag);
        *slot = tag->next_in_hash;

        DllRemove(tag);
        FreeTag(tag);
    }
}

function Tag *
PushTagsWithName(Arena *arena, Project *project, String name)
{
    HashResult hash = HashString(name);

    Tag *result = nullptr;

    Tag *tag = project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
    while (tag)
    {
        if (tag->hash == hash)
        {
            Buffer *buffer = GetBuffer(tag->buffer);

            String tag_name = PushBufferRange(arena, buffer, MakeRangeStartLength(tag->pos, tag->length));
            if (AreEqual(name, tag_name))
            {
                Tag *new_result = PushStruct(arena, Tag);
                CopyStruct(tag, new_result);
                new_result->next         = nullptr;
                new_result->prev         = nullptr;
                new_result->next_in_hash = nullptr;
                SllStackPush(result, new_result);
            }
        }

        tag = tag->next_in_hash;
    }

    return result;
}

//
// Tag Parser
//

function Token *
Advance(TagParser *parser)
{
    parser->pushed_text = false;
    Token *result = parser->t;
    Token *t = Next(&parser->it);
    if (t)
    {
        parser->t = t;
    }
    else
    {
        parser->t = &parser->null_token;
    }
    return result;
}

function void
Rewind(TagParser *parser, Token *t)
{
    Rewind(&parser->it, t);
    Advance(parser);
}

function bool
TokensLeft(TagParser *parser)
{
    return parser->t != &parser->null_token;
}

function void
InitializeTagParser(TagParser *parser, Buffer *buffer)
{
    ZeroStruct(parser);
    parser->buffer         = buffer;
    parser->it             = IterateTokens(buffer);
    parser->t              = &parser->null_token;
    parser->text_container = MakeStringContainer(ArrayCount(parser->text_container_storage), parser->text_container_storage);
    Advance(parser); // warm up first token
}

function void
SetFlags(TagParser *parser, TokenFlags require_flags, TokenFlags reject_flags)
{
    parser->require_flags = require_flags;
    parser->reject_flags  = reject_flags;
}

function String
GetTokenText(TagParser *parser)
{
    if (!parser->pushed_text)
    {
        parser->pushed_text = true;
        Clear(&parser->text_container);
        PushTokenString(&parser->text_container, parser->buffer, parser->t);
    }
    String result = parser->text_container.as_string;
    return result;
}

function Token *
PeekToken(TagParser *parser)
{
    return parser->t;
}

function Token *
MatchNextToken(TagParser *parser, TokenKind kind, String match_text = {})
{
    Token *result = nullptr;
    Token *t = PeekNext(&parser->it, 0);
    if (t && t->kind == kind)
    {
        result = t;
        if (match_text.size > 0)
        {
            String text = GetTokenText(parser);
            if (!AreEqual(match_text, text))
            {
                result = nullptr;
            }
        }
    }
    if (result)
    {
        if (((result->flags & parser->require_flags) != parser->require_flags) ||
            (result->flags & parser->reject_flags))
        {
            result = nullptr;
        }
    }
    return result;
}

function Token *
MatchToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {})
{
    Token *result = nullptr;
    if (parser->t->kind == kind &&
        (!sub_kind || parser->t->sub_kind == sub_kind))
    {
        if (match_text.size > 0)
        {
            String text = GetTokenText(parser);
            if (AreEqual(match_text, text))
            {
                result = parser->t;
            }
        }
        else
        {
            result = parser->t;
        }
    }
    if (result)
    {
        if (((result->flags & parser->require_flags) != parser->require_flags) ||
            (result->flags & parser->reject_flags))
        {
            result = nullptr;
        }
    }
    return result;
}

function Token *
MatchToken(TagParser *parser, TokenKind kind, String match_text = {})
{
    return MatchToken(parser, kind, 0, match_text);
}

function Token *
ConsumeToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {})
{
    Token *result = MatchToken(parser, kind, sub_kind, match_text);
    if (result)
    {
        Advance(parser);
    }
    return result;
}

function Token *
ConsumeToken(TagParser *parser, TokenKind kind, String match_text = {})
{
    Token *result = MatchToken(parser, kind, match_text);
    if (result)
    {
        Advance(parser);
    }
    return result;
}

function bool
ConsumeBalancedPair(TagParser *parser, TokenKind left_kind)
{
    Token *rewind = parser->t;

    if (Token *t = MatchToken(parser, left_kind))
    {
        NestHelper helper = {};
        while (IsInNest(&helper, PeekToken(parser)->kind, Direction_Forward))
        {
            Advance(parser);
        }

        return true;
    }

    Rewind(parser, rewind);

    return false;
}

//
//
//

function void
ParseTags(Buffer *buffer)
{
    LanguageSpec *lang = buffer->language;
    if (lang->ParseTags)
    {
        lang->ParseTags(buffer);
    }
}
