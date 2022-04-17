function bool
HandleCompletionMenuEvent(CompletionMenu *menu, PlatformEvent *event)
{
    if (menu->count <= 0) 
        return false;

    bool handled_any = false;

    if (event->type == PlatformEvent_KeyDown)
    {
        handled_any = true;
        switch (event->input_code)
        {
            case PlatformInputCode_Down:
            {
                menu->at_index++;
                if (menu->at_index >= menu->count)
                {
                    menu->at_index = 0;
                }
            } break;

            case PlatformInputCode_Up:
            {
                menu->at_index--;
                if (menu->at_index < 0)
                {
                    menu->at_index = menu->count - 1;
                }
            } break;

            case PlatformInputCode_Tab:
            {
            } break;

            default:
            {
                handled_any = false;
            } break;
        }
    }

    return handled_any;
}

function void
AddEntry(CompletionMenu *menu, const CompletionEntry &entry)
{
    menu->entries[menu->count++] = entry;
}
