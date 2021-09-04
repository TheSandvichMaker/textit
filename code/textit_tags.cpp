function Tag *
AllocateTag(Buffer *buffer)
{
    if (!buffer->first_free_tag)
    {
        buffer->first_free_tag = PushStructNoClear(&buffer->arena, Tag);
        buffer->first_free_tag->next = nullptr;
    }
    Tag *result = buffer->first_free_tag;
    buffer->first_free_tag = result->next;

    ZeroStruct(result);

    return result;
}

function void
FreeTag(Buffer *buffer, Tag *tag)
{
    tag->next = buffer->first_free_tag;
    buffer->first_free_tag = tag;
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

    Tag *result = AllocateTag(buffer);

    HashResult hash = HashString(name);
    result->hash = hash;

    if (!project->opening)
    {
        Tag **slot = &project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
        result->next_in_hash = *slot;
        *slot = result;
    }

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
    while (DllHasNodes(&tags->sentinel))
    {
        Tag *tag = tags->sentinel.next;

        Tag **slot = GetTagSlot(project, tag);
        Assert(*slot == tag);
        *slot = tag->next_in_hash;

        DllRemove(tag);
        tag->next = tag->prev = nullptr;

        FreeTag(buffer, tag);
    }
}

function Tag *
PushTagsWithName(Arena *arena, Project *project, String name)
{
    HashResult hash = HashString(name);

    Arena *temp = platform->GetTempArena();

    Tag *result = nullptr;

    Tag *tag = project->tag_table[hash.u32[0] % ArrayCount(project->tag_table)];
    while (tag)
    {
        if (tag->hash == hash)
        {
            Buffer *buffer = GetBuffer(tag->buffer);
            String tag_name = PushBufferRange(temp, buffer, MakeRangeStartLength(tag->pos, tag->length));
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

function TokenLocator
GetLocator(TagParser *parser)
{
    return GetLocator(&parser->it);
}

function void
Rewind(TagParser *parser, TokenLocator locator)
{
    Rewind(&parser->it, locator);
}

function bool
TokensLeft(TagParser *parser)
{
    return IsValid(&parser->it);
}

function bool
AcceptToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, Token *t)
{
    bool result = t->kind == kind && (!sub_kind || t->sub_kind == sub_kind);
    if (((t->flags & parser->require_flags) != parser->require_flags ||
         (t->flags & parser->reject_flags)))
    {
        result = false;
    }
    return result;
}

function void
Advance(TagParser *parser)
{
    parser->parse_index += 1;
    parser->pushed_text = false;
    Next(&parser->it);
}

function void
InitializeTagParser(TagParser *parser, Buffer *buffer)
{
    ZeroStruct(parser);
    parser->buffer         = buffer;
    parser->it             = IterateTokens(buffer);
    parser->text_container = MakeStringContainer(ArrayCount(parser->text_container_storage), parser->text_container_storage);
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
        PushTokenString(&parser->text_container, parser->buffer, &parser->it.token);
    }
    String result = parser->text_container.as_string;
    return result;
}

function Token
PeekToken(TagParser *parser)
{
    return parser->it.token;
}

function Token
MatchToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {})
{
    Token t = parser->it.token;
    if (AcceptToken(parser, kind, sub_kind, &t))
    {
        if (match_text.size > 0)
        {
            String text = GetTokenText(parser);
            if (AreEqual(match_text, text))
            {
                return t;
            }
        }
        else
        {
            return t;
        }
    }
    return {};
}

function Token
MatchToken(TagParser *parser, TokenKind kind, String match_text = {})
{
    return MatchToken(parser, kind, 0, match_text);
}

function Token
ConsumeToken(TagParser *parser, TokenKind kind, TokenSubKind sub_kind, String match_text = {})
{
    Token result = MatchToken(parser, kind, sub_kind, match_text);
    if (result.kind)
    {
        Advance(parser);
    }
    return result;
}

function Token
ConsumeToken(TagParser *parser, TokenKind kind, String match_text = {})
{
    Token result = MatchToken(parser, kind, match_text);
    if (result.kind)
    {
        Advance(parser);
    }
    return result;
}

function bool
ConsumeBalancedPair(TagParser *parser, TokenKind left_kind)
{
    TokenLocator rewind = GetLocator(parser);

    Token t = MatchToken(parser, left_kind);
    if (t.kind)
    {
        NestHelper helper = {};
        while (IsInNest(&helper, parser->it.token.kind, Direction_Forward))
        {
            Advance(parser);
            if (!TokensLeft(parser))
            {
                return false;
            }
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

function void
ParseBuiltinTag(TagParser *parser)
{
    Buffer *buffer = parser->buffer;

    SetFlags(parser, TokenFlag_IsComment, 0);
    if (ConsumeToken(parser, '@'))
    {
        if (Token t = ConsumeToken(parser, Token_Identifier))
        {
            Tag *tag = AddTag(buffer, &t);
            tag->kind = Tag_CommentAnnotation;
            return;
        }
    }

    Advance(parser);
}
