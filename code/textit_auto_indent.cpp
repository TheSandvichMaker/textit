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
    if (first_token->pos >= line_end) first_token = nullptr;

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    int nest_stack_at = 0;
    TokenKind *nest_stack = PushArrayNoClear(arena, 257, TokenKind) + 1;

    Token *anchor = nullptr;
    Token *t      = Prev(&it);

    bool unfinished_statement = false;
    if (!(rules->table[t->kind] & IndentRule_AffectsIndent) &&
        (t->flags & TokenFlag_LastInLine) &&
        ((t->kind != Token_StatementEnd) &&
         (t->kind != ',')))
    {
        unfinished_statement = true;
    }

    while (t)
    {
        if (t->pos >= line_end) continue;
        if (t->pos >= line_start) continue;

        IndentRule state = rules->table[t->kind];
        if (state & IndentRule_EndAny)
        {
            if (nest_stack_at < 256)
            {
                nest_stack[nest_stack_at++] = GetOtherNestTokenKind(t->kind);
            }
            else
            {
                INVALID_CODE_PATH;
            }
        }

        if (state & IndentRule_BeginAny)
        {
            if (nest_stack[nest_stack_at - 1] == t->kind)
            {
                nest_stack_at -= 1;
            }
            else
            {
                anchor = t;
                break;
            }
        }

        t = Prev(&it);
    }

    int64_t indent_width = core_config->indent_width;
    int64_t indent       = 0;
    int64_t align        = 0;

    if (anchor)
    {
        int64_t start = FindLineStart(buffer, anchor->pos);

        int64_t anchor_depth = 0;
        int64_t indent_depth = 0;
        bool finished_counting_indent = false;

        for (int64_t at = start; at < anchor->pos && IsInBufferRange(buffer, at); at += 1)
        {
            uint8_t c = ReadBufferByte(buffer, at);
            if (c == ' ')
            {
                anchor_depth += 1;
            }
            else if (c == '\t')
            {
                anchor_depth += indent_width;
            }
            else
            {
                anchor_depth += 1;
                if (!finished_counting_indent)
                {
                    finished_counting_indent = true;
                    indent_depth = anchor_depth;
                }
            }
        }

        if (!finished_counting_indent)
        {
            indent_depth = anchor_depth;
        }

        int64_t anchor_delta = anchor_depth - indent_depth;

        indent = indent_depth;

        IndentRule anchor_state = rules->table[anchor->kind];
        if (anchor_state & IndentRule_BeginAny)
        {
            if (first_token &&
                (first_token->kind == GetOtherNestTokenKind(anchor->kind)))
            {
                if (anchor_state & IndentRule_Regular)
                {
                    /* indent = indent_depth; */
                }
                else if (anchor_state & IndentRule_Hanging)
                {
                    align = anchor_delta;
                }
                unfinished_statement = false; // TODO: feels like this could be handled "more elegantly"?
            }
            else if (anchor_state & IndentRule_BeginRegular)
            {
                if (anchor_state & IndentRule_Additive)
                {
                    indent = indent_depth + indent_width;
                    align = anchor_delta;
                }
                else
                {
                    indent = indent_depth + indent_width;
                }
            }
            else if (anchor_state & IndentRule_BeginHanging)
            {
                align = anchor_delta + anchor->length;
            }
        }
    }

    if (unfinished_statement)
    {
        indent += indent_width;
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

    spaces += align;

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

function void
LoadDefaultIndentRules(IndentRules *rules)
{
    ZeroStruct(rules);
    rules->unfinished_statement      = IndentRule_BeginRegular;
    rules->table[Token_LeftParen]    = IndentRule_BeginHanging;
    rules->table[Token_RightParen]   = IndentRule_EndHanging;
    rules->table[Token_LeftScope]    = IndentRule_Additive|IndentRule_BeginRegular;
    rules->table[Token_RightScope]   = IndentRule_Additive|IndentRule_EndRegular;
    rules->table[Token_Preprocessor] = IndentRule_ForceLeft;
}

function void
LoadOtherIndentRules(IndentRules *rules)
{
    ZeroStruct(rules);
    rules->unfinished_statement      = IndentRule_BeginRegular;
    rules->table[Token_LeftParen]    = IndentRule_BeginRegular;
    rules->table[Token_RightParen]   = IndentRule_EndRegular;
    rules->table[Token_LeftScope]    = IndentRule_BeginRegular;
    rules->table[Token_RightScope]   = IndentRule_EndRegular;
    rules->table[Token_Preprocessor] = IndentRule_ForceLeft;
}
