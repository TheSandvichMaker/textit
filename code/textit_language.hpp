#ifndef TEXTIT_LANGUAGE_HPP
#define TEXTIT_LANGUAGE_HPP

union KeywordSlot
{
    struct
    {
        uint64_t id   : 56;
        uint64_t kind : 8;
    };
    uint64_t value;
};

struct LanguageSpec
{
    LanguageSpec *next;

    String name;

    int associated_extension_count;
    String associated_extensions[16];

    bool allow_nested_block_comments;

    KeywordSlot keyword_table[128];
};

#endif /* TEXTIT_LANGUAGE_HPP */
