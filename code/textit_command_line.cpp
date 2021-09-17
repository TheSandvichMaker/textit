function String 
GetCommandString(CommandLine *cl)
{
    return TrimSpaces(MakeString(cl->count, cl->text));
}

function void
EndCommandLine()
{
    if (editor->command_line_count > 0)
    {
        CommandLine *cl = editor->command_lines[--editor->command_line_count];
        if (cl->OnTerminate && !cl->accepted_entry) cl->OnTerminate(cl);

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
        dest->text             = PushString(cl->arena, prediction.text);
        dest->preview_text     = (prediction.preview_text.size ? PushString(cl->arena, prediction.preview_text) : dest->text);
        dest->color            = prediction.color;
        dest->quickselect_char = ToUpperAscii(prediction.quickselect_char);
        dest->incomplete       = prediction.incomplete;
        dest->userdata         = prediction.userdata;
        cl->sort_keys[index].key = sort_key;
        return true;
    }
    cl->prediction_overflow = true;
    return false;
}

function Prediction *
GetPrediction(CommandLine *cl, int index)
{
    if (index == -1) index = cl->prediction_selected_index;
    if (index <  0)                    index = 0;
    if (index >= cl->prediction_count) index = cl->prediction_count - 1;
    return &cl->predictions[cl->sort_keys[index].index];
}

function bool
HandleCommandLineEvent(CommandLine *cl, PlatformEvent *event)
{
    bool handled_any_events = false;

    auto AcceptPrediction = [](CommandLine *cl)
    {
        if (cl->prediction_selected_index == -1) return;
        if (!cl->prediction_count) return;

        Prediction *prediction = GetPrediction(cl, cl->prediction_selected_index);

        cl->count = (int)prediction->text.size;
        CopyArray(cl->count, prediction->text.data, cl->text);

        cl->cursor = cl->count;
    };

    if (MatchFilter(event->type, PlatformEventFilter_Text|PlatformEventFilter_Keyboard))
    {
        bool handled_event = true;
        if (event->type == PlatformEvent_Text)
        {
            String text = GetText(event);

            bool handled_quickselect = false;
            for (int i = 0; i < cl->prediction_count; i += 1)
            {
                Prediction *pred = &cl->predictions[i];
                if (pred->quickselect_char == ToUpperAscii(text[0]))
                {
                    cl->prediction_selected_index = i;
                    AcceptPrediction(cl);
                    bool terminate = cl->AcceptEntry(cl);
                    if (terminate && !editor->changed_command_line)
                    {
                        EndAllCommandLines();
                    }
                    handled_quickselect = true;
                    break;
                }
            }

            if (!handled_quickselect)
            {
                for (size_t i = 0; i < text.size; i += 1)
                {
                    if (IsPrintableAscii(text.data[i]))
                    {
                        AcceptPrediction(cl);
                        break;
                    }
                }

                if (cl->OnText && text[0] != '\n')
                {
                    text = cl->OnText(cl, text);
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
        }
        else if (event->type == PlatformEvent_KeyDown)
        {
            cl->cycling_predictions = false;

            switch (event->input_code)
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
                    if (event->input_code >= '1' &&
                        event->input_code <= '9')
                    {
                        selection = (int)event->input_code - '1';
                    }
                    else if (event->ctrl_down)
                    {
                        selection = 9 + (int)event->input_code - 'A';
                    }

                    if (selection == -1)
                    {
                        handled_event = false;
                        break;
                    }

                    if (cl->no_quickselect && !event->ctrl_down)
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

                    if (cl->AcceptEntry)
                    {
                        bool terminate = cl->AcceptEntry(cl);
                        cl->accepted_entry = terminate;
                        if (terminate && !editor->changed_command_line)
                        {
                            EndAllCommandLines();
                        }
                    }
                    else
                    {
                        cl->accepted_entry = true;
                        EndAllCommandLines();
                    }
                } break;

                case PlatformInputCode_Up:
                case PlatformInputCode_Down:
                case PlatformInputCode_Tab:
                {
                    bool going_down = ((event->input_code == PlatformInputCode_Down) ||
                                       (event->input_code == PlatformInputCode_Tab && event->shift_down));
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
                    if (event->ctrl_down)
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
        else if (event->type == PlatformEvent_KeyUp)
        {
            switch (event->input_code)
            {
                case PlatformInputCode_Control:
                {
                    cl->highlight_numbers = false;
                } break;
            }
        }

        CommandLine *top_cl = editor->command_lines[editor->command_line_count - 1];

        auto ClPostEvent = [](CommandLine *cl)
        {
            if (!cl->cycling_predictions &&
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

            if (cl->terminate)
            {
                EndAllCommandLines();
            }
        };

        if (handled_event && top_cl)
        {
            ClPostEvent(top_cl);
        }

        handled_any_events |= handled_event;
    }

    editor->changed_command_line = false;

    return handled_any_events;
}
