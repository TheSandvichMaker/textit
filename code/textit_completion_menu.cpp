function void
Reset(CompletionMenu *menu)
{
    Clear(&menu->arena);

    menu->active = false;
    menu->at_index = 0;
    menu->count = 0;
    menu->capacity = 16;
    menu->overflow = false;
    ConfigReadI32("[completion]/max_items"_str, &menu->capacity);

    menu->entries = PushArray(&menu->arena, menu->capacity, CompletionEntry);
}

function bool
HandleCompletionMenuEvent(CompletionMenu *menu, PlatformEvent *event)
{
    if (menu->count <= 0) 
        return false;

    if (!menu->active)
        return false;

    bool handled_any = false;

    View *view = GetActiveView();
    Buffer *buffer = GetActiveBuffer();
    Cursor *cursor = GetCursor(view, buffer);

    int at_index = menu->at_index;

    if (event->type == PlatformEvent_KeyDown)
    {
        handled_any = true;
        switch (event->input_code)
        {
            case PlatformInputCode_Alt:
            case PlatformInputCode_Control:
            case PlatformInputCode_Shift:
            {
                // eat silently
            } break;

            case PlatformInputCode_Down:
            {
                menu->at_index++;
            } break;

            case PlatformInputCode_Up:
            {
                menu->at_index--;
            } break;

            case PlatformInputCode_Tab:
            {
                if (event->shift_down)
                {
                    menu->at_index--;
                }
                else
                {
                    menu->at_index++;
                }
                editor->suppress_text_event = true;
            } break;

            case PlatformInputCode_Escape:
            {
                DeactivateCompletion(menu);
                handled_any = false;
            } break;

            case PlatformInputCode_Return:
            {
                CompletionEntry *entry = &menu->entries[menu->at_index];
                BufferReplaceRange(buffer, menu->replace_range, entry->string);

                SetCursor(cursor, menu->replace_range.start + entry->cursor_pos, menu->replace_range, menu->replace_range);

                editor->suppress_text_event = true;
                menu->count = 0;
                menu->active = false;
            } break;

            default:
            {
                handled_any = false;
            } break;
        }
    }

    if (menu->active)
    {
        if (menu->at_index >= menu->count)
        {
            menu->at_index = 0;
        }

        if (menu->at_index < 0)
        {
            menu->at_index = menu->count - 1;
        }

        if (at_index != menu->at_index)
        {
            CompletionEntry *preview_entry = &menu->entries[menu->at_index];
            Range new_range = MakeRangeStartLength(menu->replace_range.start, preview_entry->string.size);

            BufferReplaceRange(buffer, menu->replace_range, preview_entry->string);
            menu->replace_range = new_range;

            SetCursor(cursor, new_range.end);
        }

        if (event->type == PlatformEvent_KeyDown && !handled_any)
        {
            FindCompletionCandidatesAt(buffer, cursor->pos);
        }
    }

    return handled_any;
}

function void
AddEntry(CompletionMenu *menu, const CompletionEntry &source_entry)
{
    if (menu->count < menu->capacity)
    {
        int edit_distance = CalculateEditDistance(menu->query, source_entry.string);

        bool already_exists = false;
        size_t insert_pos = 0;
        for (; insert_pos < menu->count; insert_pos++)
        {
            CompletionEntry *other = &menu->entries[insert_pos];

            if (AreEqual(source_entry.string, other->string))
            {
                already_exists = true;
                break;
            }

            if (edit_distance < other->edit_distance)
                break;
        }

        if (!already_exists)
        {
            MoveArray(menu->entries + insert_pos, menu->entries + menu->count, menu->entries + insert_pos + 1);

            CompletionEntry *entry = &menu->entries[insert_pos];
            CopyStruct(&source_entry, entry);

            entry->desc          = PushString(&menu->arena, entry->desc);
            entry->string        = PushString(&menu->arena, entry->string);
            entry->edit_distance = edit_distance;

            if (menu->count < menu->capacity)
                menu->count++;
        }
    }
    else
    {
        menu->overflow = true;
    }
}

function bool
TriggerCompletion(CompletionMenu *menu)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view, buffer);

    FindCompletionCandidatesAt(buffer, cursor->pos);

    if (menu->count > 0)
    {
        menu->active = true;
    }
    else
    {
        Reset(menu);
    }

    return menu->active;
}

function void
DeactivateCompletion(CompletionMenu *menu)
{
    Buffer *buffer = GetActiveBuffer();
    BufferReplaceRange(buffer, menu->replace_range, menu->query);

    menu->active = false;
    Reset(menu);
}

function void
FindCompletionCandidatesAt(Buffer *buffer, int64_t pos)
{
    CompletionMenu *menu = &editor->completion;
    Reset(menu);

    Selection sel = ScanWordBackward(buffer, pos);
    Range range = SanitizeRange(sel.outer);

    String string = PushBufferRange(&menu->arena, buffer, range);
    menu->query = string;
    menu->replace_range = range;

    LanguageSpec *language = buffer->language;

    if (string.size > 0)
    {
        for (BufferIterator it = IterateBuffers();
             IsValid(&it);
             Next(&it))
        {
            Tags *tags = it.buffer->tags;
            for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
            {
                ScopedMemory temp;
                String name = PushBufferRange(temp, it.buffer, MakeRangeStartLength(tag->pos, tag->length));

                if (name.size != string.size && MatchPrefix(name, string, StringMatch_CaseInsensitive))
                {
                    String kind_name     = GetTagBaseKindName(tag);
                    String sub_kind_name = GetTagSubKindName(buffer->language, tag);

                    CustomAutocompleteResult custom = language->CustomAutocomplete(temp, tag, name);

                    CompletionEntry entry = {};
                    entry.desc   = PushStringF(temp, "%-32.*s %.*s %.*s", StringExpand(name), StringExpand(sub_kind_name), StringExpand(kind_name));
                    entry.string = custom.text; 
                    entry.cursor_pos = custom.pos;
                    AddEntry(menu, entry);
                }
            }
        }
    }

    if (menu->count == 0)
    {
        DeactivateCompletion(menu);
    }
}
