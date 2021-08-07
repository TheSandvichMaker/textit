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
        editor->changed_command_line = true;
    }

    CommandLine *cl = PushStruct(&editor->command_arena, CommandLine);
    cl->arena = PushSubArena(&editor->command_arena, Kilobytes(512));
    cl->prediction_selected_index = -1;
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
        if (cl->prediction_selected_index == -1) return;
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
                    AcceptPrediction(cl);
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
    // View *view = GetActiveView();

    Color text_foreground           = GetThemeColor("text_foreground"_id);
    Color text_background           = GetThemeColor("command_line_background"_id);
    Color color_name                = GetThemeColor("command_line_name"_id);
    Color color_selected            = GetThemeColor("command_line_option_selected"_id);
    Color color_numbers             = GetThemeColor("command_line_option_numbers"_id);
    Color color_numbers_highlighted = GetThemeColor("command_line_option_numbers_highlighted"_id);
    Color overlay_background        = GetThemeColor("command_line_background"_id);

    if (cl->highlight_numbers)
    {
        color_numbers = color_numbers_highlighted;
    }

    int64_t total_width  = 0;
    int64_t total_height = cl->prediction_count;

    V2i p = MakeV2i(render_state->viewport.min.x, render_state->viewport.max.y - 1);

    V2i text_p = p;
    if (cl->name.size)
    {
        PushText(Layer_OverlayText, text_p, PushTempStringF("%.*s ", StringExpand(cl->name)), color_name, text_background);
        text_p.x += cl->name.size + 1;
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
                PushText(Layer_OverlayText, p + MakeV2i(0, -prediction_offset), PushTempStringF("%c ", c), color_numbers, overlay_background);
            }

            PushText(Layer_OverlayText, p + MakeV2i(2, -prediction_offset), text, color, overlay_background);

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

    PushText(Layer_OverlayText, text_p, text, text_foreground, text_background);
    PushTile(Layer_OverlayText, text_p + MakeV2i(cursor_at, 0), MakeSprite(' ', text_background, text_foreground));

    /* 
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
    */
}
