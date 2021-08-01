function void
EndCommandLine()
{
    editor->command_line = nullptr;
    Clear(&editor->command_arena);
}

function CommandLine *
BeginCommandLine()
{
    if (editor->command_line)
    {
        EndCommandLine();
        editor->changed_command_line = true;
    }

    CommandLine *cl = PushStruct(&editor->command_arena, CommandLine);
    cl->arena = PushSubArena(&editor->command_arena, Kilobytes(512));
    editor->command_line = cl;
    return cl;
}

function bool
AddPrediction(CommandLine *cl, const Prediction &prediction)
{
    if (cl->prediction_count < ArrayCount(cl->predictions))
    {
        Prediction *dest = &cl->predictions[cl->prediction_count++];
        dest->text         = PushString(cl->arena, prediction.text);
        dest->preview_text = (prediction.preview_text.size ? PushString(cl->arena, prediction.preview_text) : dest->text);
        dest->color        = prediction.color;
        dest->incomplete   = prediction.incomplete;
        return true;
    }
    return false;
}

function bool
HandleCommandLineEvent(CommandLine *cl, const PlatformEvent &event)
{
    bool handled_any_events = false;

    Assert(cl->AcceptEntry);

    auto AcceptPrediction = [](CommandLine *cl)
    {
        if (!cl->prediction_count) return;

        const Prediction &prediction = cl->predictions[cl->prediction_selected_index];
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

            bool yes = false;
            for (size_t i = 0; i < text.size; i += 1)
            {
                // TODO: I need to surpress text events from handled key presses so I don't have garbage like that != ':'. That's nonsense.
                if (IsPrintableAscii(text.data[i]) && text.data[i] != ':')
                {
                    yes = true;
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

            if (yes)
            {
                if (cl->prediction_selected_index != -1) AcceptPrediction(cl);
            }
        }
        else if (event.type == PlatformEvent_KeyDown)
        {
            cl->cycling_predictions = false;

            switch (event.input_code)
            {
                case PlatformInputCode_Left:
                {
                    if (cl->cursor > 0)
                    {
                        cl->cursor -= 1;
                    }
                } break;

                case PlatformInputCode_Right:
                {
                    if (cl->cursor < cl->count)
                    {
                        cl->cursor += 1;
                    }
                } break;

                case PlatformInputCode_Escape:
                {
                    EndCommandLine();
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
                        break;
                    }

                    if (selection <= cl->prediction_count)
                    {
                        cl->prediction_selected_index = selection;
                        Prediction *prediction = &cl->predictions[cl->prediction_selected_index];
                        if (prediction->incomplete)
                        {
                            AcceptPrediction(cl);
                            break;
                        }
                    }
                } /*** FALLTHROUGH ***/
                case PlatformInputCode_Return:
                {
                    if (cl->prediction_selected_index == -1) cl->prediction_selected_index = 0;
                    AcceptPrediction(cl);

                    bool terminate = cl->AcceptEntry(cl);
                    if (terminate && !editor->changed_command_line)
                    {
                        EndCommandLine();
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
                    if (cl->prediction_selected_index != -1) AcceptPrediction(cl);
                    size_t left = ArrayCount(cl->text) - cl->cursor;
                    if (cl->cursor > 0)
                    {
                        memmove(cl->text + cl->cursor - 1,
                                cl->text + cl->cursor,
                                left);
                        cl->cursor -= 1;
                        cl->count  -= 1;
                    }
                } break;

                case PlatformInputCode_Delete:
                {
                    if (cl->prediction_selected_index != -1) AcceptPrediction(cl);
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

        if (handled_event &&
            !cl->cycling_predictions &&
            cl->GatherPredictions)
        {
            cl->prediction_count = 0;
            cl->prediction_index = 0;
            cl->prediction_selected_index = -1;

            Clear(cl->arena);
            cl->GatherPredictions(cl);
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
DrawCommandLine(CommandLine *cl)
{
    View *view = GetActiveView();

    Color text_foreground    = GetThemeColor("text_foreground"_id);
    Color text_background    = GetThemeColor("text_background"_id);
    Color color_name         = GetThemeColor("command_line_name"_id);
    Color color_selected     = GetThemeColor("command_line_option_selected"_id);
    Color color_numbers      = GetThemeColor("command_line_option_numbers"_id);
    Color overlay_background = GetThemeColor("text_background_unreachable"_id);

    int64_t total_width  = 0;
    int64_t total_height = cl->prediction_count;

    V2i p = MakeV2i(view->viewport.min.x + 2, view->viewport.max.y - 1);
    V2i text_p = p;
    if (cl->name.size)
    {
        text_p = DrawLine(p, cl->name, color_name, text_background);
        PushTile(Layer_OverlayText, text_p, MakeSprite(' ', text_foreground, text_background));
        text_p.x += 1;
    }

    if (cl->prediction_count > 0)
    {
        int prediction_offset = 1;
        for (int i = 0; i < cl->prediction_count; i += 1)
        {
            Prediction *prediction = &cl->predictions[i];
            String text = prediction->preview_text;

            Color color = GetThemeColor(prediction->color ? prediction->color : "command_line_option"_id);
            if (i == cl->prediction_selected_index)
            {
                color = color_selected;
            }

            if (i < 9 + 26)
            {
                int c = (i < 9 ? '1' + i : 'A' + i - 9);
                PushTile(Layer_OverlayText, p + MakeV2i(0, -prediction_offset), MakeSprite(c, color_numbers, overlay_background));
                PushTile(Layer_OverlayText, p + MakeV2i(1, -prediction_offset), MakeSprite(' ', color, overlay_background));
            }

            for (size_t j = 0; j < text.size; j += 1)
            {
                Sprite sprite = MakeSprite(text.data[j], color, overlay_background);
                PushTile(Layer_OverlayText, p + MakeV2i(2 + j, -prediction_offset), sprite);
            }

            total_width = Max(total_width, 2 + (int64_t)text.size);

            prediction_offset += 1;
        }
    }

    PushRect(Layer_Overlay, MakeRect2iMinDim(p + MakeV2i(0, -total_height), MakeV2i(total_width, total_height)), overlay_background);

    int cursor_at = cl->cursor;
    String text = MakeString(cl->count, cl->text);
    if (cl->prediction_selected_index != -1)
    {
        text = cl->predictions[cl->prediction_selected_index].text;
        cursor_at = (int)text.size;
    }

    for (int i = 0; i < text.size + 1; i += 1)
    {
        Sprite sprite;
        if (i < text.size)
        {
            sprite = MakeSprite(text.data[i], text_foreground, text_background);
        }
        else
        {
            sprite = MakeSprite(0, text_foreground, text_background);
        }
        if (i == cursor_at)
        {
            Swap(sprite.foreground, sprite.background);
        }
        PushTile(Layer_OverlayText, text_p + MakeV2i(i, 0), sprite);
    }
}
