function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    int64_t  target_indent_column = 0;
    int64_t  indent_width         = core_config->indent_width;

    LineData *line_data = &buffer->null_line_data;
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos, &line_data);

    int64_t line_start = loc.line_range.start;
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    TokenIterator it = MakeTokenIterator(line_data);

    bool force = false;

    TokenFlags ignored_flags = TokenFlag_IsPreprocessor|TokenFlag_IsComment;

    Token *t = it.token;
    if (t->pos < loc.line_range.end)
    {
        // this is the token we're currently writing

        if (t->kind == Token_RightScope)
        {
            target_indent_column -= indent_width;
        }
        else if (t->kind == Token_Preprocessor)
        {
            force = true;
            target_indent_column = 0;
            ignored_flags &= ~TokenFlag_IsPreprocessor;
        }
    }

    if (!force)
    {
        Token   *anchor          = nullptr;
        Token   *prev_line_token = nullptr;
        int64_t  scope_depth     = 0;
        int64_t  paren_depth     = 0;

        for (;;)
        {
            t = Prev(&it);
            if (!t) break;

            if (!HasFlag(t->flags, ignored_flags))
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
                    target_indent_column += 1;
                }
                else if (c == '\t')
                {
                    target_indent_column += indent_width;
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
                    target_indent_column = anchor->pos - line_range.start + 1;
                }
                else
                {
                    Assert(core_config->scope_indent_style == IndentStyle_Regular);
                    target_indent_column += indent_width;
                }
            }

            if (anchor->kind == Token_LeftParen)
            {
                if (core_config->paren_indent_style == IndentStyle_Hanging)
                {
                    target_indent_column = anchor->pos - line_range.start + 1;
                }
                else
                {
                    Assert(core_config->paren_indent_style == IndentStyle_Regular);
                    target_indent_column += indent_width;
                }
            }
        }
    }

    target_indent_column = Max(0, target_indent_column);

    int64_t tabs   = 0;
    int64_t spaces = 0;
    if (core_config->indent_with_tabs)
    {
        tabs   = target_indent_column / indent_width;
        spaces = target_indent_column % indent_width;
    }
    else
    {
        spaces = target_indent_column;
    }

    String string = PushStringSpace(platform->GetTempArena(), tabs + spaces);

    size_t at = 0;
    for (int64_t i = 0; i < tabs; i += 1)
    {
        string.data[at++] = '\t';
    }
    for (int64_t i = 0; i < spaces; i += 1)
    {
        string.data[at++] = ' ';
    }
    Assert(at == string.size);

    int64_t result = BufferReplaceRange(buffer, MakeRange(line_start, first_non_whitespace), string);
    return result;
}
