function void 
AddSnippet(const SnippetDesc &desc)
{
    HashResult hash = HashString(desc.query);
    uint64_t slot = hash.u64[0] % SNIPPET_TABLE_SIZE;

    Snippet *snippet = PushStruct(&editor->transient_arena, Snippet);
    snippet->desc = desc;

    snippet->next_in_hash = snippets->table[slot];
    snippets->table[slot] = snippet;
}

function Snippet *
FindSnippet(String query)
{
    Snippet *result = nullptr;

    HashResult hash = HashString(query);
    uint64_t slot = hash.u64[0] % SNIPPET_TABLE_SIZE;

    for (Snippet *snippet = snippets->table[slot];
         snippet;
         snippet = snippet->next_in_hash)
    {
        if (AreEqual(query, snippet->desc.query))
        {
            result = snippet;
            break;
        }
    }

    return result;
}

function Snippet *
TryFindSnippet(Buffer *buffer, int64_t pos)
{
    ScopedMemory temp;

    Snippet *result = nullptr;

    int64_t line_start = FindLineStart(buffer, pos);
    String code = PushBufferRange(temp, buffer, MakeRange(line_start, pos));

    for (size_t i = 0; i < code.size; i++)
    {
        String sub = Substring(code, code.size - i - 1);
        Snippet *snip = FindSnippet(sub);
        if (snip)
        {
            result = snip;
            break;
        }
    }

    return result;
}

function void
AddSubstitution(StringList *list, String string)
{
#if 0
    if (MatchPrefix(string, "core_config"_str))
    {
        string = Advance(string, sizeof("core_config") - 1);
        if (string[0] == ':')
        {
            String variable = Substring(string, 1);
            const MemberInfo *member = FindMember<CoreConfig>(variable);
            if (member)
            {
                PushString(list, temp, FormatMember(member, GetMemberPointer(core_config, member)));
            }
            else
            {
                PushString(list, temp, "!!ERROR - unknown config variable!!"_str);
            }
        }
    }
    else
    {
        PushString(list, temp, "!!ERROR - unknown query!!"_str);
    }
#else
    size_t colon_at = Find(string, ':');
    if (colon_at != string.size)
    {
        String rhs;
        String lhs = SplitAround(string, colon_at, &rhs);

        if (AreEqual(lhs, "config"_str))
        {
            String value;
            if (ConfigReadString(rhs, &value))
            {
                Push(list, value);
            }
            else
            {
                PushF(list, "<config variable '%.*s' not found>", StringExpand(rhs));
            }
        }
        else
        {
            PushF(list, "<unknown scope '%.*s'>", StringExpand(lhs));
        }
    }
    else
    {
        PushF(list, "<unknown expansion '%.*s'>", StringExpand(string));
    }
#endif
}

function String
DoSnippetStringSubstitutions(String str)
{
    ScopedMemory temp;
    StringList list = MakeStringList(temp);

    size_t start = 0;
    size_t end   = 0;
    while (end < str.size)
    {
        if (str[end] == '{')
        {
            if (start != end)
            {
                Push(&list, Substring(str, start, end));
            }
            end++;
            start = end;
            while (end < str.size)
            {
                if (str[end] == '}')
                {
                    String contents = Substring(str, start, end);
                    AddSubstitution(&list, contents);
                    end++;
                    start = end;
                    break;
                }
                else
                {
                    end++;
                }
            }
        }
        else
        {
            end++;
        }
    }

    if (start != end)
    {
        Push(&list, Substring(str, start, end));
    }

    String result = FlattenString(&list);
    return result;
}

function bool
TryApplySnippet(Buffer *buffer, int64_t pos)
{
    bool result = false;

    Snippet *snip = TryFindSnippet(buffer, pos);
    if (snip)
    {
        String replacement = DoSnippetStringSubstitutions(snip->desc.replacement);
        BufferReplaceRange(buffer, MakeRange(pos - snip->desc.query.size, pos), replacement);
        result = true;
    }

    return result;
}
