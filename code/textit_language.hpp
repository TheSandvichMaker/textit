#ifndef TEXTIT_LANGUAGE_HPP
#define TEXTIT_LANGUAGE_HPP

struct Buffer;
struct Tokenizer;
struct Tag;

union KeywordSlot
{
    struct
    {
        uint64_t id   : 56;
        uint64_t kind : 8;
    };
    uint64_t value;
};

struct OperatorSlot
{
    uint32_t      pattern;
    uint8_t       pattern_length;
    TokenKind     kind;
    TokenSubKind  sub_kind;
};

struct CustomAutocompleteResult
{
    String text;
    int64_t pos;
};

typedef void (*TokenizeHook)(Tokenizer *tok, Token *t);
typedef void (*ParseTagsHook)(Buffer *buffer);
typedef CustomAutocompleteResult (*CustomAutocompleteHook)(Arena *arena, Tag *tag, String text);

struct LanguageSpec
{
    LanguageSpec *next;

    String name;

    int associated_extension_count;
    String associated_extensions[16];

    size_t tokenize_userdata_size;

    TokenizeHook Tokenize;
    ParseTagsHook ParseTags;
    CustomAutocompleteHook CustomAutocomplete;

    StringID    sub_token_kind_to_theme_id[256];
    StringID    sub_tag_kind_to_theme_id[256];
    String      sub_tag_kind_name[256];
    KeywordSlot keyword_table[256];

    uint32_t operator_count;
    OperatorSlot operators[256];
};

function void TokenizeBasic(Tokenizer *tok, Token *t);
function CustomAutocompleteResult CustomAutocompleteBasic(Arena *arena, Tag *, String text);

struct LanguageRegistry
{
    LanguageSpec null_language;
    LanguageSpec *first_language;

    LanguageRegistry()
    {
        first_language = nullptr;

        null_language.name = "none"_str;
        null_language.Tokenize = TokenizeBasic;
        null_language.CustomAutocomplete = CustomAutocompleteBasic;
        SllStackPush(first_language, &null_language);
    }
};
GLOBAL_STATE(LanguageRegistry, language_registry);

struct LanguageRegisterHelper
{
    LanguageSpec lang = language_registry->null_language;
    LanguageRegisterHelper(String name, void (*register_proc)(LanguageSpec *))
    {
        register_proc(&lang);
        lang.name = name;
        SllStackPush(language_registry->first_language, &lang);
    }
};
#define BEGIN_REGISTER_LANGUAGE(name, language_parameter_name) \
    LanguageRegisterHelper Paste(register_language_helper_, __COUNTER__)(Paste(name, _str), [](LanguageSpec *language_parameter_name)
#define END_REGISTER_LANGUAGE );

#endif /* TEXTIT_LANGUAGE_HPP */
