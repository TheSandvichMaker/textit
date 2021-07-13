function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    int64_t indent_spaces = 0;
    int64_t indent_tabs   = 0;

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);

    int64_t line_start = loc.line_range.start;
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    TokenIterator it = MakeTokenIterator(buffer, first_non_whitespace);
    Token *anchor = nullptr;
    Token *prev_line_token = nullptr;
    int scope_depth = 0;
    int paren_depth = 0;
    for (;;)
    {
        Token *t = it.token;
        if ((t->pos < loc.line_range.end) &&
            (t->kind == Token_RightScope))
        {
            indent_spaces -= 4; // horrible
        }

        t = Prev(&it);
        if (!t) break;

        if (!HasFlag(t->flags, TokenFlag_IsPreprocessor|TokenFlag_IsComment))
        {
            if (t->kind == Token_RightParen) paren_depth += 1;
            if (t->kind == Token_RightScope) scope_depth += 1;

            if (t->kind == Token_LeftParen)
            {
                paren_depth -= 1;
                if (paren_depth < 0)
                {
                    anchor = t;
                    break;
                }
            }

            if (t->kind == Token_LeftScope)
            {
                scope_depth -= 1;
                if (scope_depth < 0)
                {
                    anchor = t;
                    break;
                }
            }

            if (!prev_line_token &&
                (paren_depth == 0) &&
                (scope_depth == 0) &&
                HasFlag(t->flags, TokenFlag_FirstInLine))
            {
                prev_line_token = t;
            }
        }
    }

    if (prev_line_token)
    {
        if (anchor)
        {
            int64_t prev_line_start = FindLineStart(buffer, prev_line_token->pos);
            if (prev_line_start > anchor->pos)
            {
                anchor = prev_line_token;
            }
        }
        else
        {
            anchor = prev_line_token;
        }
    }

    if (anchor)
    {
        Range line_range = EncloseLine(buffer, anchor->pos);
        int64_t scan_pos = line_range.start;
        while (scan_pos < line_range.end)
        {
            uint8_t c = ReadBufferByte(buffer, scan_pos);
            scan_pos += 1;

            if (c == ' ')
            {
                indent_spaces += 1;
            }
            else if (c == '\t')
            {
                indent_tabs += 1;
            }
            else
            {
                break;
            }
        }

        if (anchor->kind == Token_LeftScope)
        {
            if (core_config->scope_indent_style == IndentStyle_Hanging)
            {
                indent_spaces = anchor->pos - line_range.start + 1;
            }
            else
            {
                Assert(core_config->scope_indent_style == IndentStyle_Regular);
                indent_spaces += 4; // TODO: Get from config
            }
        }

        if (anchor->kind == Token_LeftParen)
        {
            if (core_config->paren_indent_style == IndentStyle_Hanging)
            {
                indent_spaces = anchor->pos - line_range.start + 1;
            }
            else
            {
                Assert(core_config->paren_indent_style == IndentStyle_Regular);
                indent_spaces += 4; // TODO: Get from config
            }
        }
    }

    indent_spaces = Max(0, indent_spaces);
    indent_tabs   = Max(0, indent_tabs);

    String string = PushStringSpace(platform->GetTempArena(), indent_spaces + indent_tabs);
    size_t at = 0;
    for (int64_t i = 0; i < indent_tabs; i += 1)
    {
        string.data[at++] = '\t';
    }
    for (int64_t i = 0; i < indent_spaces; i += 1)
    {
        string.data[at++] = ' ';
    }
    Assert(at == string.size);
    int64_t result = BufferReplaceRange(buffer, MakeRange(line_start, first_non_whitespace), string);
    return result;
}
