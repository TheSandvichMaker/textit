#ifndef TEXTIT_CONFIG_HPP
#define TEXTIT_CONFIG_HPP

enum ConfigAccess
{
    ConfigAccess_Root,
    ConfigAccess_User,
    ConfigAccess_Proj,
    ConfigAccess_ProjWeak,
    ConfigAccess_Any,
};

#define CONFIG_TABLE_SIZE 4096

struct ConfigVar
{
    ConfigVar *next_in_hash;
    String key;
    String value;
};

struct Config
{
    Config *next_in_hash;

    String path;
    uint64_t last_file_write_time;

    Arena arena;
    ConfigVar *table[CONFIG_TABLE_SIZE];
};

#define CONFIG_REGISTRY_SIZE 128

struct ConfigRegistry
{
    Config root;
    Config *user;

    Arena arena;
    Config *table[CONFIG_REGISTRY_SIZE];
};

GLOBAL_STATE(ConfigRegistry, config_registry);

function void InitConfigs();
function void HotReloadConfigs();

function void ReadConfigFromString(Config *config, String string);
function void ReadConfigFromFile(Config *config, String path);

function Config *ConfigFromFile(String path, bool make_if_not_exist);

function Config *GetConfig(ConfigAccess access = ConfigAccess_Any);

function bool ConfigReadStringRaw(Config *cfg, String key, String *value);
function bool ConfigReadString(String key, String *value, ConfigAccess access = ConfigAccess_Any);
function bool ConfigReadI64(String key, int64_t *value, ConfigAccess access = ConfigAccess_Any);

function void ConfigWriteStringRaw(Config *cfg, String key, String value);
function void ConfigWriteString(String key, String value, ConfigAccess access = ConfigAccess_Any);
function void ConfigWriteI64(String key, int64_t value, ConfigAccess access = ConfigAccess_Any);

#endif /* TEXTIT_CONFIG_HPP */
