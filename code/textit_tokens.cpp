//
// Token Locator
//

function TokenLocator
LocateTokenAtPos(LineInfo *info, int64_t pos)
{
    TokenLocator result = {};

    Assert(pos >= info->range.start);

    int64_t at_pos = info->range.start;
    for (TokenBlock *block = info->data->first_token_block;
         block;
         block = block->next)
    {
        for (int64_t index = 0; index < block->token_count; index += 1)
        {
            Token *token = &block->tokens[index];
            if (token->kind != Token_Whitespace && (at_pos + token->length > pos))
            {
                result.block = block;
                result.index = index;
                result.pos   = at_pos;
                return result;
            }
            at_pos += token->length;
        }
    }

    return result;
}

function bool
ValidateTokenLocatorIntegrity(Buffer *buffer, TokenLocator locator)
{
    if (!IsValid(locator))
    {
        return true;
    }

    LineInfo info;
    FindLineInfoByPos(buffer, locator.pos, &info);
    TokenLocator test_locator = LocateTokenAtPos(&info, locator.pos);
    
    Assert(test_locator.block == locator.block);
    Assert(test_locator.index == locator.index);
    Assert(test_locator.pos   == locator.pos);

    return true;
}

//
// Token Iterator
//

function void
Rewind(TokenIterator *it, TokenLocator locator)
{
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
}

function TokenIterator
IterateTokens(Buffer *buffer, int64_t pos = 0)
{
    TokenIterator result = {};

    LineInfo info;
    FindLineInfoByPos(buffer, pos, &info);

    TokenLocator locator = LocateTokenAtPos(&info, pos);
    Rewind(&result, locator);

#if TEXTIT_SLOW
    result.buffer = buffer;
    ValidateTokenLocatorIntegrity(buffer, locator);
#endif

    return result;
}

function TokenIterator
IterateLineTokens(Buffer *buffer, int64_t line)
{
    TokenIterator result = {};

    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    TokenLocator locator = LocateTokenAtPos(&info, info.range.start);
    Rewind(&result, locator);

#if TEXTIT_SLOW
    result.buffer = buffer;
    ValidateTokenLocatorIntegrity(buffer, locator);
#endif

    return result;
}

function TokenIterator
IterateLineTokens(LineInfo *info)
{
    TokenIterator result = {};

    TokenLocator locator = LocateTokenAtPos(info, info->range.start);
    Rewind(&result, locator);

#if TEXTIT_SLOW
    result.buffer = DEBUG_FindWhichBufferThisMemoryBelongsTo(info->data);
    ValidateTokenLocatorIntegrity(result.buffer, locator);
#endif

    return result;
}

function bool
IsValid(TokenIterator *it)
{
    return it->block && it->index >= 0 && it->index < it->block->token_count;
}

function bool
IsValidForward(TokenIterator *it)
{
    return it->block && (it->block->next || it->index < it->block->token_count - 1);
}

function bool
IsValidBackward(TokenIterator *it)
{
    return it->block && (it->block->prev || it->index > 0);
}

function TokenLocator
GetLocator(TokenIterator *it)
{
    TokenLocator locator = {};
    locator.block = it->block;
    locator.index = it->index;
    locator.pos   = it->token.pos;
    return locator;
}

function TokenLocator
LocateNext(TokenIterator *it, int offset = 1)
{
    if (!IsValid(it)) return {};

    TokenLocator result = {};

    TokenBlock *block  = it->block;
    int64_t     index  = it->index;
    int64_t     pos    = it->token.pos;
    int64_t     length = it->token.length;

    for (;;)
    {
        Assert(block->token_count != TOKEN_BLOCK_FREE_TAG);

        pos += length;

        offset -= 1;
        index  += 1;
        while (index >= block->token_count)
        {
            if (block->next)
            {
                block = block->next;
                index = 0;
            }
            else
            {
                break;
            }
        }

        if (index >= 0 && index < block->token_count)
        {
            Token *t = &block->tokens[index];
            length = t->length;

            if (offset <= 0 && t->kind != Token_Whitespace)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    result.block = block;
    result.index = index;
    result.pos   = pos;

#if TEXTIT_SLOW
    ValidateTokenLocatorIntegrity(it->buffer, result);
    it->iteration_index += 1;
#endif

    return result;
}

function Token
PeekNext(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocateNext(it, offset);
    Token token = GetToken(locator);
    return token;
}

function Token
Next(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocateNext(it, offset);
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
    return it->token;
}

function TokenLocator
LocatePrev(TokenIterator *it, int offset = 1)
{
    if (!IsValid(it)) return {};

    TokenLocator result = {};

    TokenBlock *block  = it->block;
    int64_t     index  = it->index;
    int64_t     pos    = it->token.pos;
    int64_t     length = it->token.length;

    for (;;)
    {
        Assert(block->token_count != TOKEN_BLOCK_FREE_TAG);

        offset -= 1;
        index  -= 1;
        while (index < 0)
        {
            if (block->prev)
            {
                block = block->prev;
                index = block->token_count - 1;
            }
            else
            {
                break;
            }
        }

        if (index >= 0 && index < block->token_count)
        {
            Token *t = &block->tokens[index];
            length = t->length;

            pos -= length;

            if (offset <= 0 && t->kind != Token_Whitespace)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    result.block = block;
    result.index = index;
    result.pos   = pos;

#if TEXTIT_SLOW
    ValidateTokenLocatorIntegrity(it->buffer, result);
    it->iteration_index += 1;
#endif

    return result;
}

function Token
PeekPrev(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocatePrev(it, offset);
    Token token = GetToken(locator);
    return token;
}

function Token
Prev(TokenIterator *it, int offset = 1)
{
    TokenLocator locator = LocatePrev(it, offset);
    it->block = locator.block;
    it->index = locator.index;
    it->token = GetToken(locator);
    return it->token;
}

function Token
EncloseString(TokenLocator locator)
{
    TokenIterator it = {};
    Rewind(&it, locator):

    Token result = {};

    if (it.token & TokenFlag_PartOfString)
    {
        TokenFlags flags = it.token.flags; // TODO: I am assuming the flags stay constant for the string.
                                           // This is a false assumption, and I should instead collect flags
                                           // from all the tokens that make up this string.

        while (IsValid(&it) && it.token.kind != Token_StringStart)
        {
            Prev(&it);
        }
        if (it.token.kind == Token_StringStart)
        {
            int64_t start = it.token.pos + it.token.length; // getting the inner range, just the contents of the string
            int8_t  inner_start_offset = it.token.inner_start_offset;
            while (IsValid(&it) && it.token.kind != Token_StringEnd)
            {
                Next(&it);
            }
            if (it.token.kind == Token_StringEnd)
            {
                int64_t end              = it.token.pos; // getting the inner range, just the contents of the string
                int8_t  inner_end_offset = it.token.inner_end_offset;

                result.kind               = Token_String;
                result.flags              = flags;
                result.length             = SafeTruncateI64ToI16(end - start);
                result.inner_start_offset = inner_start_offset;
                result.inner_end_offset   = inner_end_offset;
                result.pos                = start;
            }
        }
    }

    return result;
}

function Token
GetTokenAt(Buffer *buffer, int64_t pos, GetTokenFlags flags = 0)
{
    Token result = {};
    TokenIterator it = IterateTokens(buffer, pos);

    if (HasFlag(it.token.flags, TokenFlag_PartOfString) &&
        HasFlag(flags, GetToken_CaptureStrings))
    {
        result = EncloseString(GetLocator(&it));
    }
    else
    {
        result = it.token;
    }

    return result;
}

//
// Nest Helper
//

function void
Clear(NestHelper *nests)
{
    ZeroStruct(nests);
}

function bool
IsInNest(NestHelper *nests, TokenKind t, Direction dir)
{
    if (nests->depth < 0) return false;
    bool result = nests->depth > 0;

    if (nests->opener)
    {
        if (dir == Direction_Forward && IsNestOpener(t))
        {
            nests->last_seen_opener = t;
        }
        else if (dir == Direction_Backward && IsNestCloser(t))
        {
            if (t != GetOtherNestTokenKind(nests->last_seen_opener))
            {
                // something smells fishy, time to give up
                Clear(nests);
                return false;
            }
        }

        if (t == nests->opener)
        {
            nests->depth += 1;
        }
        else if (t == nests->closer)
        {
            nests->depth -= 1;
            if (nests->depth == 0)
            {
                Clear(nests);
                nests->depth = -1;
            }
        }
    }
    else
    {
        bool starts_nest = false;
        if (dir == Direction_Forward)  starts_nest = IsNestOpener(t);
        if (dir == Direction_Backward) starts_nest = IsNestCloser(t);
        if (starts_nest)
        {
            nests->opener = t;
            nests->closer = GetOtherNestTokenKind(t);
            nests->depth += 1;

            result = true;
        }
    }

    return result;
}

