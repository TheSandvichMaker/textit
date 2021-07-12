function int64_t
AutoIndentLineAt(Buffer *buffer, int64_t pos)
{
    int64_t indent_spaces = 0;
    int64_t indent_tabs   = 0;

    int64_t line_start = FindLineStart(buffer, pos);
    int64_t first_non_whitespace = FindFirstNonHorzWhitespace(buffer, line_start);

    TokenIterator it = MakeTokenIterator(buffer, first_non_whitespace);
    for (;;)
    {
        Token *t = it.token;
        if (t->kind == Token_RightScope ||
            t->kind == Token_RightParen)
        {
            indent_spaces -= 4; // TODO: Get from config
        }

        t = Prev(&it);
        if (!t) break;

        if (!HasFlag(t->flags, TokenFlag_IsPreprocessor|TokenFlag_IsComment))
        {
            Range line_range = EncloseLine(buffer, t->pos);
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

            if (t->kind == Token_LeftScope ||
                t->kind == Token_LeftParen)
            {
                indent_spaces += 4; // TODO: Get from config
            }

            break;
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
