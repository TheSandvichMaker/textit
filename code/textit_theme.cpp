function void
SetThemeColor(StringID key, Color color)
{
    Theme *theme = &editor->theme;

    for (size_t i = 0; i < MAX_THEME_COLORS; i += 1)
    {
        size_t slot_index = (key + i) % MAX_THEME_COLORS;
        ThemeColor *slot = &theme->map[slot_index];
        Assert(slot->key != key);
        if (!slot->key)
        {
            slot->key   = key;
            slot->color = color;
            break;
        }
    }
}

function Color
GetThemeColor(StringID key)
{
    Color result = MakeColor(255, 0, 255);

    Theme *theme = &editor->theme;
    for (size_t i = 0; i < MAX_THEME_COLORS; i += 1)
    {
        size_t slot_index = (key + i) % MAX_THEME_COLORS;
        ThemeColor *slot = &theme->map[slot_index];
        if (slot->key == key)
        {
            result = slot->color;
            break;
        }
    }

    return result;
}

function void
LoadDefaultTheme()
{
    SetThemeColor("text_identifier"_id, MakeColor(255, 255, 255));
    SetThemeColor("text_keyword"_id, MakeColor(255, 192, 0));
    SetThemeColor("text_flowcontrol"_id, MakeColor(255, 128, 0));
    SetThemeColor("text_preprocessor"_id, MakeColor(255, 192, 255));
    SetThemeColor("text_string"_id, MakeColor(255, 192, 64));
    SetThemeColor("text_number"_id, MakeColor(64, 192, 255));
    SetThemeColor("text_literal"_id, MakeColor(64, 128, 255));
    SetThemeColor("text_line_comment"_id, MakeColor(0, 192, 0));
    SetThemeColor("text_type"_id, MakeColor(64, 255, 192));
    SetThemeColor("text_comment"_id, MakeColor(0, 192, 0));
    SetThemeColor("text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor("text_foreground_dim"_id, MakeColor(192, 192, 192));
    SetThemeColor("text_foreground_dimmer"_id, MakeColor(128, 128, 128));
    SetThemeColor("text_foreground_dimmest"_id, MakeColor(4*12, 4*20, 4*32));
    SetThemeColor("text_background"_id, MakeColor(12, 20, 32));
    SetThemeColor("text_background_unreachable"_id, MakeColor(24, 40, 64));
    SetThemeColor("text_background_highlighted"_id, MakeColor(48, 80, 128));
    SetThemeColor("filebar_text_foreground"_id, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_id, MakeColor(192, 255, 128));
    SetThemeColor("filebar_text_inactive"_id, MakeColor(128, 128, 128));
    SetThemeColor("filebar_text_background_text_mode"_id, MakeColor(255, 192, 128));
    SetThemeColor("filebar_text_background_clutch"_id, MakeColor(64, 128, 255));
    SetThemeColor("unrenderable_text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor("unrenderable_text_background"_id, MakeColor(192, 0, 0));
    SetThemeColor("inner_selection_background"_id, MakeColor(32, 96, 128));
    SetThemeColor("outer_selection_background"_id, MakeColor(128, 96, 32));
    SetThemeColor("line_highlight"_id, MakeColor(18, 30, 48));
}
