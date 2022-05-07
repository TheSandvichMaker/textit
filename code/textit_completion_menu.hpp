#ifndef TEXTIT_COMPLETION_MENU_HPP
#define TEXTIT_COMPLETION_MENU_HPP

struct CompletionEntry
{
    String desc;
    String string;
    int edit_distance;
    int64_t cursor_pos;
};

struct CompletionMenu
{
    Arena arena;
    bool active;
    int at_index;
    int count;
    int capacity;
    CompletionEntry *entries;
    String query;
    Range replace_range;
    bool overflow;
};

function void Reset(CompletionMenu *menu);
function bool HandleCompletionMenuEvent(CompletionMenu *menu, PlatformEvent *event);
function void AddEntry(CompletionMenu *menu, const CompletionEntry &entry);
function bool TriggerCompletion(CompletionMenu *menu);
function void DeactivateCompletion(CompletionMenu *menu);
function void FindCompletionCandidatesAt(Buffer *buffer, int64_t pos);

#endif /* TEXTIT_COMPLETION_MENU_HPP */
