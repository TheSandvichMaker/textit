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

//
// New config API
//

function void
InitConfigs()
{
    config_registry->user = ConfigFromFile(CombinePath(platform->GetTempArena(), platform->GetExeDirectory(), "user.textit"_str), true);
}

function void
HotReloadConfigs()
{
    for (int i = 0; i < CONFIG_REGISTRY_SIZE; i++)
    {
        for (Config *cfg = config_registry->table[i]; cfg; cfg = cfg->next_in_hash)
        {
            uint64_t last_write_time = platform->GetLastFileWriteTime(cfg->path);
            if (cfg->last_file_write_time != last_write_time)
            {
                cfg->last_file_write_time = last_write_time;
                ReadConfigFromFile(cfg, cfg->path);
            }
        }
    }
}

function void
ReadConfigFromString(Config *config, String string)
{
    Release(&config->arena);
    ZeroArray(ArrayCount(config->table), &config->table[0]);

    while (string.size)
    {
        String line = TrimSpaces(SplitLine(string, &string));

        if (!line.size || line[0] == '#')
            continue;

        String key = SplitWord(line, &line);
        String value = TrimSpaces(SplitAround(line, '#', &line));

        if (value.size >= 2 && value[0] == '"' && value[value.size - 1] == '"')
        {
            value = Substring(value, 1, value.size - 1);
        }

        ConfigWriteStringRaw(config, key, value);
    }
}

function void
ReadConfigFromFile(Config *config, String path)
{
    ScopedMemory temp;

    String file = platform->ReadFile(temp, path);
    if (file.size)
    {
        ReadConfigFromString(config, file);
    }
}

function Config *
ConfigFromFile(String path, bool make_if_not_exist)
{
    ScopedMemory temp;

    Config *result = nullptr;

    uint64_t index = HashString(path).u64[0] % CONFIG_REGISTRY_SIZE;
    for (Config *cfg = config_registry->table[index]; cfg; cfg = cfg->next_in_hash)
    {
        if (AreEqual(cfg->path, path))
        {
            result = cfg;
            break;
        }
    }

    if (!result)
    {
        String file = platform->ReadFile(temp, path);
        if (file.size || make_if_not_exist)
        {
            result = PushStruct(&config_registry->arena, Config);
            result->path = PushString(&config_registry->arena, path);

            result->next_in_hash = config_registry->table[index];
            config_registry->table[index] = result;
        }

        if (file.size)
        {
            ReadConfigFromString(result, file);
        }
    }

    return result;
}

function Config *
GetConfig(ConfigAccess access)
{
    ScopedMemory temp;

    Config *result = nullptr;

    switch (access)
    {
        case ConfigAccess_Root: { result = &config_registry->root; } break;
        case ConfigAccess_User: { result = config_registry->user; } break;

        case ConfigAccess_Any:
        case ConfigAccess_Proj: 
        case ConfigAccess_ProjWeak:
        { 
            Buffer *buffer = GetActiveBuffer();
            String path = CombinePath(temp, buffer->project->root, "project.textit"_str);
            result = ConfigFromFile(path, access == ConfigAccess_Proj);
        } break;
    }

    if (!result && access == ConfigAccess_Any)
    {
        result = config_registry->user;
    }

    return result;
}

function bool
ConfigReadStringRaw(Config *cfg, String key, String *value)
{
    bool result = false;

    uint64_t index = HashString(key).u64[0] % CONFIG_TABLE_SIZE;
    
    for (ConfigVar *var = cfg->table[index]; var; var = var->next_in_hash)
    {
        if (AreEqual(key, var->key))
        {
            *value = var->value;
            result = true;
            break;
        }
    }

    return result;
}

function bool
ConfigReadString(String key, String *value, ConfigAccess access)
{
    bool result = false;
    if (access == ConfigAccess_Any)
    {
        Config *cfg = GetConfig(ConfigAccess_ProjWeak);
        if (cfg)
        {
            result = ConfigReadStringRaw(cfg, key, value);
        }

        if (!result)
        {
            cfg = GetConfig(ConfigAccess_User);
            result = ConfigReadStringRaw(cfg, key, value);
        }

        if (!result)
        {
            cfg = GetConfig(ConfigAccess_Root);
            result = ConfigReadStringRaw(cfg, key, value);
        }
    }
    else
    {
        Config *cfg = GetConfig(access);
        result = ConfigReadStringRaw(cfg, key, value);

        if (!result && access == ConfigAccess_User)
        {
            cfg = GetConfig(ConfigAccess_Root);
            result = ConfigReadStringRaw(cfg, key, value);
        }
    }
    return result;
}

function bool
ConfigReadI64(String key, int64_t *value, ConfigAccess access)
{
    bool result = false;

    String str;
    if (ConfigReadString(key, &str, access))
    {
        result = ParseInt(str, nullptr, value);
    }

    return result;
}

function void
ConfigWriteStringRaw(Config *cfg, String key, String value)
{
    uint64_t index = HashString(key).u64[0] % CONFIG_TABLE_SIZE;

    ConfigVar *var = nullptr;
    for (ConfigVar *at = cfg->table[index]; at; at = at->next_in_hash)
    {
        if (AreEqual(at->key, key))
        {
            var = at;
            break;
        }
    }

    if (!var)
    {
        var = PushStruct(&cfg->arena, ConfigVar);
        var->key = PushString(&cfg->arena, key);

        var->next_in_hash = cfg->table[index];
        cfg->table[index] = var;
    }

    var->value = PushString(&cfg->arena, value);
}

function void
ConfigWriteString(String key, String value, ConfigAccess access)
{
    Config *cfg = GetConfig(access);

    if (cfg)
    {
        ConfigWriteStringRaw(cfg, key, value);
    }
}

function void
ConfigWriteI64(String key, int64_t value, ConfigAccess access)
{
    String str = PushTempStringF("%lli", value);
    ConfigWriteString(key, str, access);
}
