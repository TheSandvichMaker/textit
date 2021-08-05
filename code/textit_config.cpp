function void
WriteMember(void *data, const MemberInfo *member, void *value)
{
    CopySize(member->size, value, (char *)data + member->offset);
};

function bool
ParseAndWriteMember(void *data, const MemberInfo *member, String val)
{
    bool success = true;

    switch (member->type)
    {
        case "bool"_id:
        {
            if (AreEqual(val, "true"_str))
            {
                bool result = true;
                WriteMember(data, member, &result);
            }
            else if (AreEqual(val, "false"_str))
            {
                bool result = false;
                WriteMember(data, member, &result);
            }
        } break;

        case "int"_id:
        case "int64_t"_id:
        {
            union
            {
                int     result_int;
                int64_t result_int64_t;
            };
            if (ParseInt(val, nullptr, &result_int64_t))
            {
                if (member->type == "int"_id)     WriteMember(data, member, &result_int);
                if (member->type == "int64_t"_id) WriteMember(data, member, &result_int64_t);
            }
        } break;

        case "String"_id:
        {
            String result = val;
            WriteMember(data, member, &result);
        } break;

        default:
        {
            success = false;
        } break;
    }
    
    return success;
}

template <typename T>
function void
ParseConfig(T *data, String string)
{
    size_t member_count       = Introspection<T>::member_count;
    const MemberInfo *members = Introspection<T>::members;

    while (string.size > 0)
    {
        String line = SplitLine(string, &string);
        String key  = TrimSpaces(SplitWord(line, &line));
        String val  = TrimSpaces(SplitAroundChar(line, '#', &line));
        for (size_t i = 0; i < member_count; i += 1)
        {
            const MemberInfo *member = &members[i];
            String short_name = SplitWord(member->name);
            if (AreEqual(short_name, key))
            {
                if (!ParseAndWriteMember(data, member, val))
                {
                    platform->LogPrint(PlatformLogLevel_Warning, "Failed to parse value \"%.*s\" for member \"%.*s\" in config",
                                   StringExpand(val),
                                   StringExpand(key));
                }
                break;
            }
        }
    }
}

function String
FormatMember(const MemberInfo *info, void *value_at)
{
    Arena *arena = platform->GetTempArena();
    String result = "?"_str;
    switch (info->type)
    {
        case "bool"_id:
        {
            bool value = *(bool *)value_at;
            result = PushStringF(arena, "%s", value ? "true" : "false");
        } break;

        case "int"_id:
        {
            result = PushStringF(arena, "%d", *(int *)value_at);
        } break;

        case "int64_t"_id:
        {
            result = PushStringF(arena, "%lld", *(int64_t *)value_at);
        } break;

        case "String"_id:
        {
            String value = *(String *)value_at;
            result = PushString(arena, value);
        } break;
    }
    return result;
}
