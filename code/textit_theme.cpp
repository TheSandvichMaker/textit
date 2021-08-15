function void
SetThemeColor(StringID key, Color color, TextStyleFlags style)
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
            slot->style = style;
            break;
        }
    }
}

function ThemeColor *
GetThemeColorInternal(StringID key)
{
    ThemeColor *result = nullptr;

    Theme *theme = &editor->theme;
    for (size_t i = 0; i < MAX_THEME_COLORS; i += 1)
    {
        size_t slot_index = (key + i) % MAX_THEME_COLORS;
        ThemeColor *slot = &theme->map[slot_index];
        if (slot->key == key)
        {
            result = slot;
            break;
        }
    }

    return result;
}

function Color
GetThemeColor(StringID key)
{
    Color result = MakeColor(255, 0, 255);

    ThemeColor *slot = GetThemeColorInternal(key);
    if (slot)
    {
        result = slot->color;
    }

    return result;
}

function TextStyleFlags
GetThemeStyle(StringID key)
{
    TextStyleFlags result = 0;

    ThemeColor *slot = GetThemeColorInternal(key);
    if (slot)
    {
        result = slot->style;
    }

    return result;
}

function void
LoadDefaultTheme()
{
    SetThemeColor("text_identifier"_id, MakeColor(255, 255, 255));
    SetThemeColor("text_keyword"_id, MakeColor(255, 192, 0));
    SetThemeColor("text_flowcontrol"_id, MakeColor(255, 128, 0), TextStyle_Bold);
    SetThemeColor("text_label"_id, MakeColor(255, 128, 0), TextStyle_Underline|TextStyle_Italic);
    SetThemeColor("text_preprocessor"_id, MakeColor(255, 192, 255));
    SetThemeColor("text_string"_id, MakeColor(255, 192, 64));
    SetThemeColor("text_number"_id, MakeColor(64, 192, 255));
    SetThemeColor("text_literal"_id, MakeColor(64, 128, 255));
    SetThemeColor("text_function"_id, MakeColor(128, 196, 255));
    SetThemeColor("text_line_comment"_id, MakeColor(0, 192, 0), TextStyle_Italic);
    SetThemeColor("text_comment"_id,      MakeColor(0, 192, 0), TextStyle_Italic);
    SetThemeColor("text_type"_id, MakeColor(64, 255, 192));
    SetThemeColor("text_foreground"_id, MakeColor(235, 235, 225));
    SetThemeColor("text_foreground_dim"_id, MakeColor(192, 192, 192));
    SetThemeColor("text_foreground_dimmer"_id, MakeColor(128, 128, 128));
    SetThemeColor("text_foreground_dimmest"_id, MakeColor(4*12, 4*20, 4*32));
    SetThemeColor("text_background"_id, MakeColor(12, 20, 32));
    SetThemeColor("text_background_inactive"_id, MakeColor(12, 20, 32));
    SetThemeColor("text_background_unreachable"_id, MakeColor(24, 40, 64));
    SetThemeColor("text_background_highlighted"_id, MakeColor(48, 80, 128));
    SetThemeColor("unrenderable_text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor("unrenderable_text_background"_id, MakeColor(192, 0, 0));

    SetThemeColor("filebar_text_foreground"_id, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_id, MakeColor(192, 255, 128));
    SetThemeColor("filebar_text_inactive"_id, MakeColor(128, 128, 128));
    SetThemeColor("filebar_text_background_text_mode"_id, MakeColor(255, 192, 128));
    SetThemeColor("filebar_text_background_clutch"_id, MakeColor(64, 128, 255));

    SetThemeColor("inner_selection_background"_id, MakeColor(32, 96, 128));
    SetThemeColor("outer_selection_background"_id, MakeColor(128, 96, 32));
    SetThemeColor("line_highlight"_id, MakeColor(18, 30, 48));

    SetThemeColor("command_line_foreground"_id, MakeColor(235, 235, 225));
    SetThemeColor("command_line_background"_id, MakeColor(6, 10, 16));
    SetThemeColor("command_line_name"_id, MakeColor(192, 128, 128));
    SetThemeColor("command_line_option"_id, MakeColor(164, 164, 164));
    SetThemeColor("command_line_option_selected"_id, MakeColor(192, 128, 128));
    SetThemeColor("command_line_option_numbers"_id, MakeColor(128, 192, 128));
    SetThemeColor("command_line_option_numbers_highlighted"_id, MakeColor(128, 255, 128));
    SetThemeColor("command_line_option_directory"_id, MakeColor(192, 192, 128));
}

function void
LoadDefaultLightTheme()
{
    SetThemeColor("text_identifier"_id, MakeColor(12, 12, 12));
    SetThemeColor("text_keyword"_id, MakeColor(192, 128, 0));
    SetThemeColor("text_flowcontrol"_id, MakeColor(255, 128, 0), TextStyle_Bold);
    SetThemeColor("text_label"_id, MakeColor(255, 128, 0), TextStyle_Underline|TextStyle_Italic);
    SetThemeColor("text_preprocessor"_id, MakeColor(192, 64, 192));
    SetThemeColor("text_string"_id, MakeColor(192, 128, 12));
    SetThemeColor("text_number"_id, MakeColor(32, 164, 192));
    SetThemeColor("text_literal"_id, MakeColor(32, 92, 192));
    SetThemeColor("text_function"_id, MakeColor(64, 100, 164));
    SetThemeColor("text_type"_id, MakeColor(12, 128, 64), TextStyle_Bold);
    SetThemeColor("text_line_comment"_id, MakeColor(12, 128, 12), TextStyle_Italic);
    SetThemeColor("text_comment"_id,      MakeColor(12, 128, 12), TextStyle_Italic);
    SetThemeColor("text_foreground"_id, MakeColor(12, 12, 12));
    SetThemeColor("text_foreground_dim"_id, MakeColor(64, 64, 64));
    SetThemeColor("text_foreground_dimmer"_id, MakeColor(128, 128, 128));
    SetThemeColor("text_foreground_dimmest"_id, MakeColor(196, 196, 196));
    SetThemeColor("text_background"_id, MakeColor(245, 245, 235));
    SetThemeColor("text_background_inactive"_id, MakeColor(245, 245, 235));
    SetThemeColor("text_background_unreachable"_id, MakeColor(225, 225, 225));
    SetThemeColor("text_background_highlighted"_id, MakeColor(200, 200, 200));
    SetThemeColor("unrenderable_text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor("unrenderable_text_background"_id, MakeColor(192, 0, 0));

    SetThemeColor("filebar_text_foreground"_id, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_id, MakeColor(92, 192, 92));
    SetThemeColor("filebar_text_inactive"_id, MakeColor(192, 192, 192));
    SetThemeColor("filebar_text_background_text_mode"_id, MakeColor(225, 128, 64));
    SetThemeColor("filebar_text_background_clutch"_id, MakeColor(64, 128, 192));

    SetThemeColor("inner_selection_background"_id, MakeColor(196, 225, 255));
    SetThemeColor("outer_selection_background"_id, MakeColor(255, 212, 196));
    SetThemeColor("line_highlight"_id, MakeColor(245, 235, 215));

    SetThemeColor("command_line_foreground"_id, MakeColor(12, 12, 12));
    SetThemeColor("command_line_background"_id, MakeColor(200, 200, 192));
    SetThemeColor("command_line_name"_id, MakeColor(192, 64, 32));
    SetThemeColor("command_line_option"_id, MakeColor(24, 24, 24));
    SetThemeColor("command_line_option_selected"_id, MakeColor(192, 32, 32));
    SetThemeColor("command_line_option_numbers"_id, MakeColor(32, 128, 32));
    SetThemeColor("command_line_option_numbers_highlighted"_id, MakeColor(32, 128, 192));
    SetThemeColor("command_line_option_directory"_id, MakeColor(48, 32, 24));
}
