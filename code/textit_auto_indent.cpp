function int64_t
RoundUpNextTabStop(int64_t col, int64_t tab_width)
{
    col = tab_width*((col + tab_width) / tab_width);
    return col;
}

function int64_t
RoundDownNextTabStop(int64_t col, int64_t tab_width)
{
    col = tab_width*((col - 1) / tab_width);
    return col;
}

function int64_t
CountIndentationDepth(Buffer *buffer, int64_t pos, int64_t indent_width)
{
    int64_t result = 0;

    for (int64_t at = pos; IsInBufferRange(buffer, at); at += 1)
    {
        uint8_t c = ReadBufferByte(buffer, at);
        if (c == ' ')
        {
            result += 1;
        }
        else if (c == '\t')
        {
            result = RoundUpNextTabStop(result, indent_width);
        }
        else
        {
            break;
        }
    }

    return result;
}

struct IndentationResult
{
    int tabs;
    int spaces;
    int columns;
    Range old_range;
};

function void
GetIndentationForLine(Buffer *buffer, int64_t line, IndentationResult *result)
{
    ZeroStruct(result);
    if (!LineIsInBuffer(buffer, line))
    {
        return;
    }

    IndentRules *rules = buffer->indent_rules;

    LineInfo info;
    FindLineInfoByLine(buffer, line, &info);

    int64_t line_start           = info.range.start;
    int64_t line_end             = info.range.end;
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    TokenIterator it = IterateTokens(buffer, first_non_whitespace);

    bool force_left = false;

    Token first_token = it.token;
    if (first_token.pos < line_end)
    {
        force_left = !!(rules->table[first_token.kind] & IndentRule_ForceLeft);
    }
    else
    {
        first_token.kind = Token_None;
    }

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    int nest_stack_at = 0;
    TokenKind *nest_stack = PushArrayNoClear(arena, 257, TokenKind) + 1;

    Token anchor = {};
    Token t      = Prev(&it);

    while (t.kind)
    {
        if (t.pos >= line_end) goto next;
        if (t.pos >= line_start) goto next;
        if (t.flags & TokenFlag_IsComment) goto next;

        IndentRule rule = rules->table[t.kind];
        if (rule & IndentRule_PopIndent)
        {
            if (nest_stack_at < 256)
            {
                nest_stack[nest_stack_at++] = GetOtherNestTokenKind(t.kind);
            }
            else
            {
                INVALID_CODE_PATH;
            }
        }
        else if (rule & IndentRule_PushIndent)
        {
            if (nest_stack[nest_stack_at - 1] == t.kind)
            {
                nest_stack_at -= 1;
            }
            else
            {
                anchor = t;
                break;
            }
        }

next:
        t = Prev(&it);
    }

    Token override_anchor = {};
    IndentRule override_rule = 0;

    IndentRule anchor_rule = rules->table[anchor.kind];

    // now we have the anchor to start with, do a forward pass for finding unfinished statements,
    // I am too stupid to do this logic in the reverse pass (or it's actually just not practical)
    for (;;)
    {
        t = Next(&it);
        if (!t.kind) break;

        if (t.pos >= line_start) break;

        IndentRule rule = rules->table[t.kind];
        // Super whacky unfinished statement detection
        if (!(anchor_rule & IndentRule_Hanging) &&
            !override_anchor.kind &&
            (t.flags & TokenFlag_LastInLine) &&
            !(t.flags & TokenFlag_IsComment) &&
            !(rule & IndentRule_StatementEnd) &&
            !(rule & IndentRule_AffectsIndent))
        {
            override_anchor = t;
            override_rule   = rules->unfinished_statement;
        }

        if (rule & IndentRule_StatementEnd)
        {
            override_anchor.kind = Token_None;
        }
    }

    if (override_anchor.kind) anchor = override_anchor;

    int64_t indent_width = core_config->indent_width;
    int64_t indent       = 0;
    int64_t align        = 0;

    if (anchor.kind)
    {
        int64_t start = FindLineStart(buffer, anchor.pos);

        int64_t anchor_depth = 0;
        int64_t indent_depth = 0;
        bool finished_counting_indent = false;

        for (int64_t at = start; at < anchor.pos && IsInBufferRange(buffer, at); at += 1)
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
                if (!finished_counting_indent)
                {
                    finished_counting_indent = true;
                    indent_depth = anchor_depth;
                }
                anchor_depth += 1;
            }
        }

        if (!finished_counting_indent)
        {
            indent_depth = anchor_depth;
        }

        int64_t anchor_delta = anchor_depth - indent_depth;

        indent = indent_depth;

        anchor_rule = rules->table[anchor.kind];
        if (override_anchor.kind)
        {
            anchor_rule = override_rule;
        }

        if (anchor_rule & IndentRule_PushIndent)
        {
            if (anchor_rule & IndentRule_Hanging)
            {
                align = anchor_delta + anchor.length;
            }
            else
            {
                if (anchor_rule & IndentRule_Additive)
                {
                    indent = indent_depth + indent_width;
                    align = anchor_delta;
                }
                else
                {
                    indent = indent_depth + indent_width;
                }
            }

            if (first_token.kind &&
                first_token.kind == GetOtherNestTokenKind(anchor.kind))
            {
                if (anchor_rule & IndentRule_Hanging)
                {
                    align = anchor_delta;
                }
                else 
                {
                    indent = indent_depth;
                    align  = 0;
                }
            }
        }
    }

    indent = Max(0, indent);
    align  = Max(0, align);

    if (force_left)
    {
        indent = align = 0;
    }

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

    result->tabs      = (short)tabs;
    result->spaces    = (short)spaces;
    result->columns   = (int)(indent + align);
    result->old_range = MakeRange(line_start, first_non_whitespace);
}

function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos); // TODO: Some redundancy here, where 
                                                                      // this function can already provide
                                                                      // the line data, but instead it is
                                                                      // re-got using the line passed below

    IndentationResult indentation;
    GetIndentationForLine(buffer, loc.line, &indentation);

    String string = PushStringSpace(platform->GetTempArena(), indentation.tabs + indentation.spaces);

    size_t at = 0;
    for (int64_t i = 0; i < indentation.tabs; i += 1)
    {
        string.data[at++] = '\t';
    }
    for (int64_t i = 0; i < indentation.spaces; i += 1)
    {
        string.data[at++] = ' ';
    }
    Assert(at == string.size);

    int64_t result = BufferReplaceRange(buffer, indentation.old_range, string);
    return result;
}

function void
LoadDefaultIndentRules(IndentRules *rules)
{
    ZeroStruct(rules);
    rules->unfinished_statement      = IndentRule_PushIndent;
    rules->table[Token_LeftParen]    = IndentRule_PushIndent|IndentRule_Hanging;
    rules->table[Token_RightParen]   = IndentRule_PopIndent;
    rules->table[Token_LeftScope]    = IndentRule_PushIndent;
    rules->table[Token_RightScope]   = IndentRule_PopIndent;
    rules->table[Token_Preprocessor] = IndentRule_ForceLeft;
    rules->table[Token_Label]        = IndentRule_ForceLeft;
    rules->table[';']                = IndentRule_StatementEnd;
    rules->table[',']                = IndentRule_StatementEnd;
}

function void
LoadOtherIndentRules(IndentRules *rules)
{
    ZeroStruct(rules);
    rules->unfinished_statement      = IndentRule_PushIndent;
    rules->table[Token_LeftParen]    = IndentRule_PushIndent;
    rules->table[Token_RightParen]   = IndentRule_PopIndent;
    rules->table[Token_LeftScope]    = IndentRule_PushIndent;
    rules->table[Token_RightScope]   = IndentRule_PopIndent;
    rules->table[Token_Preprocessor] = IndentRule_ForceLeft;
    rules->table[Token_Label]        = IndentRule_ForceLeft;
    rules->table[';']                = IndentRule_StatementEnd;
    rules->table[',']                = IndentRule_StatementEnd;
}
