#if 0
function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    IndentRules *rules = buffer->indent_rules;

    int64_t target_indent_column = 0;
    int64_t indent_width         = core_config->indent_width;

    LineData *line_data = &buffer->null_line_data;
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos, &line_data);

    int64_t line_start = loc.line_range.start;
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    TokenIterator it = MakeTokenIterator(line_data);

    bool force = false;

    TokenFlags required_flags_mask = TokenFlag_IsPreprocessor|TokenFlag_IsComment;
    TokenFlags required_flags = 0;

    Token *t = it.token;
    if (t->pos < loc.line_range.end)
    {
        // this is the token we're currently writing

        // if we're in a comment, align within comment,
        // if we're in a preprocessor directive, align within preprocessor directive
        required_flags |= t->flags & required_flags_mask;

        IndentState state = rules->states[t->kind];
        if (state & IndentState_EndRegular)
        {
            target_indent_column -= indent_width;
        }
        else if (state & IndentState_ForceLeft)
        {
            force = true;
            target_indent_column = 0;
        }
    }

    if (!force)
    {
        // find anchor

        Token   *anchor          = nullptr;
        Token   *prev_line_token = nullptr;
        int64_t  regular_depth   = 0;
        int64_t  hanging_depth   = 0;
        int64_t  unfinished_statement_depth = 0;

        for (;;)
        {
            t = Prev(&it);
            if (!t) break;

            if ((t->flags & required_flags_mask) == required_flags)
            {
                IndentState state = rules->states[t->kind];

                if (state & IndentState_EndRegular) regular_depth += 1;
                if (state & IndentState_EndHanging) hanging_depth += 1;

                if (state & IndentState_BeginRegular)
                {
                    regular_depth -= 1;
                    if (regular_depth < 0)
                    {
                        anchor = t;
                        break;
                    }
                }

                if (state & IndentState_BeginHanging)
                {
                    hanging_depth -= 1;
                    if (hanging_depth < 0)
                    {
                        anchor = t;
                        break;
                    }
                }

                if (HasFlag(t->flags, TokenFlag_LastInLine) &&
                    (t->kind == Token_StatementEnd))
                {
                    unfinished_statement_depth += 1;
                }

                if (HasFlag(t->flags, TokenFlag_LastInLine) &&
                    (t->kind != Token_StatementEnd))
                {
                    unfinished_statement_depth -= 1;
                    if (unfinished_statement_depth < 0)
                    {
                        anchor = t;
                    }
                    break;
                }

                if (!prev_line_token &&
                    (regular_depth == 0) &&
                    (hanging_depth == 0) &&
                    HasFlag(t->flags, TokenFlag_FirstInLine))
                {
                    prev_line_token = t;
                }
            }
        }

        if (prev_line_token)
        {
            // see if previous line should override the anchor

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
            // determine anchor depth

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

            // adjust depth based on anchor indent rules

            IndentState anchor_state = rules->states[anchor->kind];
            if (anchor->flags & TokenFlag_LastInLine)
            {
                anchor_state = rules->unfinished_statement;
            }

            if (anchor_state & IndentState_BeginRegular)
            {
                target_indent_column += indent_width;
            }
            else if (anchor_state & IndentState_BeginHanging)
            {
                target_indent_column = anchor->pos - line_range.start + 1;
            }
        }
    }

    target_indent_column = Max(0, target_indent_column);

    // emit indentation

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
#else
function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    IndentRules *rules = buffer->indent_rules;

    LineData *line_data = &buffer->null_line_data;
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos, &line_data);

    int64_t line_start           = loc.line_range.start;
    int64_t line_end             = loc.line_range.end;
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    // find anchor (first unbalanced opening indent token)
    TokenIterator it = MakeTokenIterator(buffer, first_non_whitespace);

    Token *first_token = it.token;

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    NestPair *first_nest  = nullptr;
    NestPair *anchor_nest = nullptr;
    Token *anchor         = nullptr;
    Token *prev_line      = nullptr;
    for (;;)
    {
        Token *t = Prev(&it);
        if (!t) break;

        if (t->pos >= line_end) continue;
        if (t->pos >= line_start) continue;

        IndentState state = rules->states[t->kind];
        if (state & IndentState_EndAny)
        {
            NestPair *nest = PushStruct(arena, NestPair);
            nest->closing_token = GetOtherNestTokenKind(t->kind);
            SllStackPush(first_nest, nest);
        }

        if (state & IndentState_BeginAny)
        {
            if (first_nest &&
                (first_nest->closing_token == t->kind))
            {
                anchor_nest = SllStackPop(first_nest);
            }
            else
            {
                anchor = t;
                break;
            }
        }

        if (!prev_line &&
            !first_nest &&
            (t->flags & TokenFlag_FirstInLine))
        {
            prev_line = t;
        }
    }

    int64_t indent_width = core_config->indent_width;
    int64_t indent       = 0;

    if (anchor)
    {
        int64_t start = FindLineStart(buffer, anchor->pos);
        int64_t depth = anchor->pos - start;

        IndentState anchor_state = rules->states[anchor->kind];
        if (anchor_state & IndentState_BeginAny)
        {
            if (first_token->kind == GetOtherNestTokenKind(anchor->kind))
            {
                depth = anchor->pos - start;
            }
            else if (anchor_state & IndentState_BeginRegular)
            {
                depth += indent_width;
            }
            else if (anchor_state & IndentState_BeginHanging)
            {
                depth += anchor->length;
            }
        }

        indent = depth;
    }

    indent = Max(0, indent);

    // emit indentation

    int64_t tabs   = 0;
    int64_t spaces = 0;
    if (core_config->indent_with_tabs)
    {
        tabs   = indent / indent_width;
        spaces = indent % indent_width;
    }
    else
    {
        spaces = indent;
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
#endif

function void
LoadDefaultIndentRules(IndentRules *rules)
{
    rules->unfinished_statement       = IndentState_BeginRegular;
    rules->states[Token_LeftParen]    = IndentState_BeginHanging;
    rules->states[Token_RightParen]   = IndentState_EndHanging;
    rules->states[Token_LeftScope]    = IndentState_BeginRegular;
    rules->states[Token_RightScope]   = IndentState_EndRegular;
    rules->states[Token_Preprocessor] = IndentState_ForceLeft;
}
