#ifndef TEXTIT_COMPLETION_MENU_HPP
#define TEXTIT_COMPLETION_MENU_HPP

struct CompletionEntry
{
    Range range;
    String string;
};

struct CompletionMenu
{
    int at_index;
    int count;
    CompletionEntry entries[128];
};

function bool HandleCompletionMenuEvent(CompletionMenu *menu, PlatformEvent *event);
function void AddEntry(CompletionMenu *menu, const CompletionEntry &entry);

#endif /* TEXTIT_COMPLETION_MENU_HPP */
