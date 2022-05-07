#ifndef TEXTIT_SNIPPET_HPP
#define TEXTIT_SNIPPET_HPP

struct SnippetDesc
{
    String description;
    String query;
    String replacement;
};

struct Snippet
{
    Snippet *next_in_hash;
    SnippetDesc desc;
};

#define SNIPPET_TABLE_SIZE 4096

struct Snippets
{
    Snippet *table[SNIPPET_TABLE_SIZE];
};

GLOBAL_STATE(Snippets, snippets);

function void AddSnippet(const SnippetDesc &desc);
function Snippet *FindSnippet(String query);
function Snippet *TryFindSnippet(Buffer *buffer, int64_t pos);
function void DoSnippetStringSubstitutions(Buffer *buffer, Range range, Snippet *snippet);
function void ApplySnippet(Buffer *buffer, Snippet *snip, int64_t pos);
function bool TryApplySnippet(Buffer *buffer, int64_t pos);

#endif /* TEXTIT_SNIPPET_HPP */
