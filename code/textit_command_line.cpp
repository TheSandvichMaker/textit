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
AddPrediction(CommandLine *cl, const String &prediction)
{
    if (cl->prediction_count < ArrayCount(cl->predictions))
    {
        cl->predictions[cl->prediction_count++] = prediction;
        return true;
    }
    return false;
}

function bool
HandleCommandLineEvent(CommandLine *cl, const PlatformEvent &event)
{
    bool handled_any_events = false;

    Assert(cl->GatherPredictions);
    Assert(cl->AcceptEntry);

    auto AcceptPrediction = [](CommandLine *cl)
    {
        cl->prediction_selected_index = cl->prediction_index++;
        cl->prediction_index %= cl->prediction_count;

        String prediction = cl->predictions[cl->prediction_selected_index];

        cl->count = (int)prediction.size;
        CopyArray(cl->count, prediction.data, cl->text);
        cl->cursor = cl->count;
    };

    if (MatchFilter(event.type, PlatformEventFilter_Text|PlatformEventFilter_Keyboard))
    {
        handled_any_events = true;

        if (event.type == PlatformEvent_Text)
        {
            String text = event.text;

            for (size_t i = 0; i < text.size; i += 1)
            {
                // TODO: I need to surpress text events from handled key presses so I don't have garbage like that != ':'. That's nonsense.
                if (IsPrintableAscii(text.data[i]) && text.data[i] != ':')
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
                INCOMPLETE_SWITCH;

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

                case PlatformInputCode_1: case PlatformInputCode_2: case PlatformInputCode_3:
                case PlatformInputCode_4: case PlatformInputCode_5: case PlatformInputCode_6:
                case PlatformInputCode_7: case PlatformInputCode_8: case PlatformInputCode_9:
                {
                    int as_digit = (int)event.input_code - '0';

                    if (cl->no_quickselect && !event.ctrl_down)
                    {
                        break;
                    }

                    if (as_digit <= cl->prediction_count)
                    {
                        cl->prediction_selected_index = as_digit - 1;
                        cl->prediction_index = (as_digit) % cl->prediction_count;
                        AcceptPrediction(cl);
                    }
                } // FALLTHROUGH
                case PlatformInputCode_Return:
                {
                    bool terminate = cl->AcceptEntry(cl);
                    if (terminate && !editor->changed_command_line)
                    {
                        EndCommandLine();
                    }
                } break;

                case PlatformInputCode_Tab:
                {
                    cl->cycling_predictions = true;
                    if (cl->prediction_count > 0)
                    {
                        AcceptPrediction(cl);
                    }
                } break;
                
                case PlatformInputCode_Back:
                {
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
            }
        }

        if (!cl->cycling_predictions)
        {
            cl->prediction_count = 0;
            cl->prediction_index = 0;
            cl->prediction_selected_index = -1;

            cl->GatherPredictions(cl);
            if (cl->prediction_index > cl->prediction_count)
            {
                cl->prediction_index = cl->prediction_count;
            }
        }
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
    Color overlay_background = GetThemeColor("text_background_unreachable"_id);

    int64_t total_width  = 0;
    int64_t total_height = cl->prediction_count;

    V2i p = MakeV2i(view->viewport.min.x + 2, view->viewport.max.y - 1);
    V2i text_p = p;
    if (cl->name.size)
    {
        text_p = DrawLine(p, cl->name, MakeColor(192, 127, 127), text_background);
        PushTile(Layer_OverlayText, text_p, MakeSprite(' ', text_foreground, text_background));
        text_p.x += 1;
    }

    if (cl->prediction_count > 0)
    {
        int prediction_offset = 1;
        for (int i = 0; i < cl->prediction_count; i += 1)
        {
            String other_prediction = cl->predictions[i];

            Color color = MakeColor(127, 127, 127);
            if (i == cl->prediction_selected_index)
            {
                color = MakeColor(192, 127, 127);
            }

            if (i < 9)
            {
                PushTile(Layer_OverlayText, p + MakeV2i(0, -prediction_offset), MakeSprite('1' + i, MakeColor(128, 192, 128), overlay_background));
                PushTile(Layer_OverlayText, p + MakeV2i(1, -prediction_offset), MakeSprite(' ', color, overlay_background));
            }

            for (size_t j = 0; j < other_prediction.size; j += 1)
            {
                Sprite sprite = MakeSprite(other_prediction.data[j], color, overlay_background);
                PushTile(Layer_OverlayText, p + MakeV2i(2 + j, -prediction_offset), sprite);
            }

            total_width = Max(total_width, 2 + (int64_t)other_prediction.size);

            prediction_offset += 1;
        }
    }

    PushRect(Layer_Overlay, MakeRect2iMinDim(p + MakeV2i(0, -total_height), MakeV2i(total_width, total_height)), overlay_background);

    for (int i = 0; i < cl->count + 1; i += 1)
    {
        Sprite sprite;
        if (i < cl->count)
        {
            sprite = MakeSprite(cl->text[i], text_foreground, text_background);
        }
        else
        {
            sprite = MakeSprite(0, text_foreground, text_background);
        }
        if (i == cl->cursor)
        {
            Swap(sprite.foreground, sprite.background);
        }
        PushTile(Layer_OverlayText, text_p + MakeV2i(i, 0), sprite);
    }
}
