function void
EndCommandLine()
{
    if (editor->command_line_count > 0)
    {
        CommandLine *cl = editor->command_lines[--editor->command_line_count];
        EndTemporaryMemory(cl->temporary_memory);

        if (editor->command_line_count > 0)
        {
            CommandLine *next_cl = editor->command_lines[editor->command_line_count - 1];
            next_cl->count  = 0;
            next_cl->cursor = 0;
        }
    }
    else
    {
        INVALID_CODE_PATH;
    }
}

function void
EndAllCommandLines()
{
    while (editor->command_line_count > 0)
    {
        EndCommandLine();
    }
}

function CommandLine *
BeginCommandLine()
{
    CommandLine *cl = nullptr; // TODO: Error handling

    int index = editor->command_line_count++;
    if (index < ArrayCount(editor->command_lines))
    {
        if (editor->command_line_count > 0)
        {
            editor->changed_command_line = true;
        }

        TemporaryMemory temp = BeginTemporaryMemory(&editor->command_arena);

        cl = PushStruct(&editor->command_arena, CommandLine);
        cl->arena                     = PushSubArena(&editor->command_arena, Kilobytes(512));
        cl->temporary_memory          = temp;
        cl->prediction_selected_index = -1;

        for (int i = 0; i < ArrayCount(cl->sort_keys); i += 1)
        {
            cl->sort_keys[i].index = i;
        }

        editor->command_lines[index] = cl;
    }

    return cl;
}

function bool
AddPrediction(CommandLine *cl, const Prediction &prediction, uint32_t sort_key)
{
    if (cl->prediction_count < ArrayCount(cl->predictions))
    {
        int index = cl->prediction_count++;
        Prediction *dest = &cl->predictions[index];
        dest->text         = PushString(cl->arena, prediction.text);
        dest->preview_text = (prediction.preview_text.size ? PushString(cl->arena, prediction.preview_text) : dest->text);
        dest->color        = prediction.color;
        dest->incomplete   = prediction.incomplete;
        cl->sort_keys[index].key = sort_key;
        return true;
    }
    cl->prediction_overflow = true;
    return false;
}

function bool
HandleCommandLineEvent(CommandLine *cl, const PlatformEvent &event)
{
    bool handled_any_events = false;

    Assert(cl->AcceptEntry);

    auto AcceptPrediction = [](CommandLine *cl)
    {
        if (cl->prediction_selected_index == -1) return;
        if (!cl->prediction_count) return;

        const Prediction &prediction = cl->predictions[cl->sort_keys[cl->prediction_selected_index].index];

        cl->count = (int)prediction.text.size;
        CopyArray(cl->count, prediction.text.data, cl->text);

        cl->cursor = cl->count;
    };

    if (MatchFilter(event.type, PlatformEventFilter_Text|PlatformEventFilter_Keyboard))
    {
        bool handled_event = true;
        if (event.type == PlatformEvent_Text)
        {
            String text = event.text;

            for (size_t i = 0; i < text.size; i += 1)
            {
                if (IsPrintableAscii(text.data[i]))
                {
                    AcceptPrediction(cl);
                    break;
                }
            }

            for (size_t i = 0; i < text.size; i += 1)
            {
                if (IsPrintableAscii(text.data[i]))
                {
                    cl->cycling_predictions = false;

                    size_t left = ArrayCount(cl->text) - cl->count;
                    if (left > 0)
                    {
                        memmove(cl->text + cl->cursor + 1,
                                cl->text + cl->cursor,
                                left);
                        cl->text[cl->cursor++] = text.data[i];
                        cl->count += 1;
                    }
                }
            }
        }
        else if (event.type == PlatformEvent_KeyDown)
        {
            cl->cycling_predictions = false;

            switch (event.input_code)
            {
                case PlatformInputCode_Left:
                {
                    AcceptPrediction(cl);
                    if (cl->cursor > 0)
                    {
                        cl->cursor -= 1;
                    }
                } break;

                case PlatformInputCode_Right:
                {
                    AcceptPrediction(cl);
                    if (cl->cursor < cl->count)
                    {
                        cl->cursor += 1;
                    }
                } break;

                case PlatformInputCode_Control:
                {
                    AcceptPrediction(cl);
                    cl->highlight_numbers = true;
                } break;

                case PlatformInputCode_Escape:
                {
                    EndAllCommandLines();
                } break;

                case 'A': case 'B': case 'C': 
                case 'D': case 'E': case 'F': 
                case 'G': case 'H': case 'I':
                case 'J': case 'K': case 'L':
                case 'M': case 'N': case 'O':
                case 'P': case 'Q': case 'R':
                case 'S': case 'T': case 'U':
                case 'V': case 'W': case 'X':
                case 'Y': case 'Z':
                case '1': case '2': case '3':
                case '4': case '5': case '6':
                case '7': case '8': case '9':
                {
                    int selection = -1;
                    if (event.input_code >= '1' &&
                        event.input_code <= '9')
                    {
                        selection = (int)event.input_code - '1';
                    }
                    else if (event.ctrl_down)
                    {
                        selection = 9 + (int)event.input_code - 'A';
                    }

                    if (selection == -1)
                    {
                        handled_event = false;
                        break;
                    }

                    if (cl->no_quickselect && !event.ctrl_down)
                    {
                        handled_event = false;
                        break;
                    }

                    if (selection < cl->prediction_count)
                    {
                        cl->prediction_selected_index = selection;
                        Prediction *prediction = &cl->predictions[cl->prediction_selected_index];
                        if (prediction->incomplete)
                        {
                            AcceptPrediction(cl);
                            break;
                        }
                    }
                    else
                    {
                        handled_event = false;
                        break;
                    }
                } /*** FALLTHROUGH ***/
                case PlatformInputCode_Return:
                {
                    if (!cl->no_autoaccept)
                    {
                        if (cl->prediction_selected_index == -1) cl->prediction_selected_index = 0;
                        AcceptPrediction(cl);
                    }

                    bool terminate = cl->AcceptEntry(cl);
                    if (terminate && !editor->changed_command_line)
                    {
                        EndAllCommandLines();
                    }
                } break;

                case PlatformInputCode_Up:
                case PlatformInputCode_Down:
                case PlatformInputCode_Tab:
                {
                    bool going_down = ((event.input_code == PlatformInputCode_Down) ||
                                       (event.input_code == PlatformInputCode_Tab && event.shift_down));
                    if (cl->prediction_count == 1)
                    {
                        cl->prediction_selected_index = 0;
                        AcceptPrediction(cl);
                    }
                    else if (cl->prediction_count > 1)
                    {
                        cl->cycling_predictions = true;
                        if (going_down)
                        {
                            cl->prediction_selected_index--;
                            if (cl->prediction_selected_index < 0)
                            {
                                cl->prediction_selected_index = cl->prediction_count - 1;
                            }
                        }
                        else
                        {
                            cl->prediction_selected_index++;
                            if (cl->prediction_selected_index >= cl->prediction_count)
                            {
                                cl->prediction_selected_index = 0;
                            }
                        }
                    }
                } break;
                
                case PlatformInputCode_Back:
                {
                    if (event.ctrl_down)
                    {
                        if (cl->count > 0)
                        {
                            cl->count = 0;
                            cl->cursor = 0;
                        }
                        else
                        {
                            EndCommandLine();
                        }
                    }
                    else
                    {
                        AcceptPrediction(cl);
                        if (cl->cursor > 0)
                        {
                            size_t left = ArrayCount(cl->text) - cl->cursor;
                            memmove(cl->text + cl->cursor - 1,
                                    cl->text + cl->cursor,
                                    left);
                            cl->cursor -= 1;
                            cl->count  -= 1;
                        }
                        else
                        {
                            EndCommandLine();
                        }
                    }
                } break;

                case PlatformInputCode_Delete:
                {
                    AcceptPrediction(cl);
                    size_t left = ArrayCount(cl->text) - cl->cursor;
                    if (cl->count > cl->cursor)
                    {
                        memmove(cl->text + cl->cursor,
                                cl->text + cl->cursor + 1,
                                left);
                        cl->count -= 1;
                        if (cl->cursor > cl->count)
                        {
                            cl->cursor = cl->count;
                        }
                    }
                } break;

                default:
                {
                    handled_event = false;
                } break;
            }
        }
        else if (event.type == PlatformEvent_KeyUp)
        {
            switch (event.input_code)
            {
                case PlatformInputCode_Control:
                {
                    cl->highlight_numbers = false;
                } break;
            }
        }

        if (handled_event &&
            !cl->cycling_predictions &&
            cl->GatherPredictions)
        {
            cl->prediction_overflow = false;
            cl->prediction_count = 0;
            cl->prediction_index = 0;
            cl->prediction_selected_index = -1;
            ZeroArray(ArrayCount(cl->sort_keys), cl->sort_keys);

            Clear(cl->arena);
            cl->GatherPredictions(cl);

            String cl_string = TrimSpaces(MakeString(cl->count, cl->text));

            SortKey *sort_keys = cl->sort_keys;
            SortKey temp_sort_keys[35];
            for (int i = 0; i < cl->prediction_count; i += 1)
            {
                Prediction *prediction = &cl->predictions[i];
                if (cl->sort_by_edit_distance)
                {
                    sort_keys[i].key = CalculateEditDistance(cl_string, prediction->preview_text);
                }
                if (!AreEqual(cl_string, prediction->preview_text, StringMatch_CaseInsensitive) &&
                    !AreEqual(cl_string, prediction->text,         StringMatch_CaseInsensitive))
                {
                    sort_keys[i].key += 100;
                }
                sort_keys[i].index = i;
            }
            if (cl_string.size > 0)
            {
                RadixSort(cl->prediction_count, sort_keys, temp_sort_keys);
            }

            if (cl->prediction_index > cl->prediction_count)
            {
                cl->prediction_index = cl->prediction_count;
            }
        }

        handled_any_events |= handled_event;
    }

    editor->changed_command_line = false;

    return handled_any_events;
}

function void
DrawCommandLines()
{
    PushLayer(Layer_OverlayForeground);

    Color text_foreground           = GetThemeColor("command_line_foreground"_id);
    Color text_background           = GetThemeColor("command_line_background"_id);
    Color color_name                = GetThemeColor("command_line_name"_id);
    Color color_option              = GetThemeColor("command_line_option"_id);
    Color color_selected            = GetThemeColor("command_line_option_selected"_id);
    Color color_numbers             = GetThemeColor("command_line_option_numbers"_id);
    Color color_numbers_highlighted = GetThemeColor("command_line_option_numbers_highlighted"_id);
    Color overlay_background        = GetThemeColor("command_line_background"_id);

    V2i p = MakeV2i(render_state->viewport.min.x, render_state->viewport.max.y - 1);

    V2i text_p = p;

    Arena *arena = platform->GetTempArena();

    StringList name_list = {};
    for (int i = 0; i < editor->command_line_count; i += 1)
    {
        CommandLine *cl = editor->command_lines[i];
        PushString(&name_list, arena, cl->name);
    }
    String names = PushFlattenedString(&name_list, arena, " > "_str, StringSeparator_AfterLast);

    text_p = DrawText(text_p, names, color_name, text_background);

    CommandLine *cl = editor->command_lines[editor->command_line_count - 1];

    int64_t total_width  = 0;
    int64_t total_height = cl->prediction_count + cl->prediction_overflow;

    if (cl->highlight_numbers)
    {
        color_numbers = color_numbers_highlighted;
    }

    Prediction *active_prediction = nullptr;

    if (cl->prediction_count > 0)
    {
        active_prediction = &cl->predictions[0];

        int prediction_offset = 1;
        for (int i = 0; i < cl->prediction_count; i += 1)
        {
            Prediction *prediction = &cl->predictions[cl->sort_keys[i].index];
            String text = prediction->preview_text;

            Color color = GetThemeColor(prediction->color ? prediction->color : "command_line_option"_id);
            if (i == cl->prediction_selected_index)
            {
                active_prediction = prediction;
                color = color_selected;
            }

            if (i < 9 + 26)
            {
                int c = (i < 9 ? '1' + i : 'A' + i - 9);
                DrawText(p + MakeV2i(0, -prediction_offset), PushTempStringF("%c ", c), color_numbers, overlay_background);
            }

            DrawText(p + MakeV2i(2, -prediction_offset), text, color, overlay_background);

            total_width = Max(total_width, 2 + (int64_t)text.size);

            prediction_offset += 1;
        }
        
        if (cl->prediction_overflow)
        {
            DrawText(p + MakeV2i(2, -prediction_offset), "... and more"_str, color_option, overlay_background);
        }
    }

    PushLayer(Layer_OverlayBackground);

    PushRect(MakeRect2iMinDim(p + MakeV2i(0, -total_height), MakeV2i(total_width, total_height)), overlay_background);

    PushLayer(Layer_OverlayForeground);

    int cursor_at = cl->cursor;
    String text = MakeString(cl->count, cl->text); if (cl->prediction_selected_index != -1)
    {
        text = cl->predictions[cl->prediction_selected_index].text;
        cursor_at = (int)text.size;
    }

    DrawText(text_p, text, text_foreground, text_background);
    DrawGlyph(text_p + MakeV2i(cursor_at, 0), ' ', text_background, text_foreground);

    if (active_prediction)
    {
        Command *cmd = FindCommand(active_prediction->text, StringMatch_CaseInsensitive);
        if (cmd != NullCommand())
        {
            String desc = cmd->description;
            DrawText(MakeV2i(render_state->viewport.max.x - desc.size, text_p.y), desc, text_foreground, text_background);
        }
    }
}
