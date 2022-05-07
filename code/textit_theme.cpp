function void
SetThemeColor(Theme *theme, StringID key, Color color, TextStyleFlags style = 0)
{
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
GetThemeColorInternal(Theme *theme, StringID key)
{
    ThemeColor *result = nullptr;

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

    ThemeColor *slot = GetThemeColorInternal(editor->theme, key);
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

    ThemeColor *slot = GetThemeColorInternal(editor->theme, key);
    if (slot)
    {
        result = slot->style;
    }

    return result;
}

function Theme *
AddTheme(String name)
{
    Theme *result = PushStruct(&editor->transient_arena, Theme);
    result->name  = PushString(&editor->transient_arena, name);
    SllQueuePush(editor->first_theme, editor->last_theme, result);
    return result;
}

function void
SetEditorTheme(String name)
{
    for (Theme *theme = editor->first_theme; theme; theme = theme->next)
    {
        if (AreEqual(name, theme->name, StringMatch_CaseInsensitive))
        {
            editor->theme = theme;
            break;
        }
    }
}

function void
LoadDefaultDarkTheme()
{
    Theme *theme = AddTheme("textit-dark"_str);
    SetThemeColor(theme, "text_identifier"_id, MakeColor(255, 255, 255));
    SetThemeColor(theme, "text_keyword"_id, MakeColor(255, 192, 0));
    SetThemeColor(theme, "text_flowcontrol"_id, MakeColor(255, 128, 0), TextStyle_Bold);
    SetThemeColor(theme, "text_label"_id, MakeColor(255, 128, 0), TextStyle_Underline|TextStyle_Italic);
    SetThemeColor(theme, "text_preprocessor"_id, MakeColor(255, 192, 255));
    SetThemeColor(theme, "text_string"_id, MakeColor(255, 192, 64));
    SetThemeColor(theme, "text_string_special"_id, MakeColor(255, 128, 64));
    SetThemeColor(theme, "text_number"_id, MakeColor(64, 192, 255));
    SetThemeColor(theme, "text_literal"_id, MakeColor(192, 212, 255));
	SetThemeColor(theme, "text_global"_id, MakeColor(245, 215, 196));
    SetThemeColor(theme, "text_function"_id, MakeColor(128, 196, 255));
    SetThemeColor(theme, "text_unknown_function"_id, MakeColor(192, 212, 255));
    SetThemeColor(theme, "text_macro"_id, MakeColor(224, 128, 224), TextStyle_Bold);
    SetThemeColor(theme, "text_function_macro"_id, MakeColor(128, 196, 255), TextStyle_Bold);
    SetThemeColor(theme, "text_comment"_id,      MakeColor(0, 164, 0), TextStyle_Italic);
    SetThemeColor(theme, "text_comment_note"_id, MakeColor(192, 255, 0), TextStyle_Italic|TextStyle_Bold|TextStyle_Underline);
    SetThemeColor(theme, "text_comment_todo"_id, MakeColor(192, 64,  0), TextStyle_Italic|TextStyle_Bold|TextStyle_Underline);
    SetThemeColor(theme, "text_comment_fixme"_id, MakeColor(192, 192,  0), TextStyle_Italic|TextStyle_Bold|TextStyle_Underline);
    SetThemeColor(theme, "text_comment_annotation"_id, MakeColor(192, 255, 0), TextStyle_Italic|TextStyle_Bold|TextStyle_Underline);
    SetThemeColor(theme, "text_type"_id, MakeColor(128, 255, 200));
    SetThemeColor(theme, "text_foreground"_id, MakeColor(235, 235, 225));
    SetThemeColor(theme, "text_foreground_dim"_id, MakeColor(192, 192, 192));
    SetThemeColor(theme, "text_foreground_dimmer"_id, MakeColor(128, 128, 128));
    SetThemeColor(theme, "text_foreground_dimmest"_id, MakeColor(4*12, 4*20, 4*32));
    SetThemeColor(theme, "text_background"_id, MakeColor(12, 20, 32));
    SetThemeColor(theme, "text_background_popup"_id, MakeColor(24, 40, 64));
    SetThemeColor(theme, "text_background_inactive"_id, MakeColor(12, 20, 32));
    SetThemeColor(theme, "text_background_unreachable"_id, MakeColor(24, 40, 64));
    SetThemeColor(theme, "text_background_highlighted"_id, MakeColor(48, 80, 128));
    SetThemeColor(theme, "text_search_highlight"_id, MakeColor(12, 96, 96));
    SetThemeColor(theme, "text_nest_highlight_foreground"_id, MakeColor(235, 235, 225));
    SetThemeColor(theme, "text_nest_highlight_background"_id, MakeColor(96, 96, 64));
    SetThemeColor(theme, "unrenderable_text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor(theme, "unrenderable_text_background"_id, MakeColor(192, 0, 0));

    SetThemeColor(theme, "filebar_text_foreground"_id, MakeColor(0, 0, 0));
    SetThemeColor(theme, "filebar_text_background"_id, MakeColor(192, 255, 128));
    SetThemeColor(theme, "filebar_text_inactive"_id, MakeColor(128, 128, 128));
    SetThemeColor(theme, "filebar_text_background_text_mode"_id, MakeColor(255, 192, 128));
    SetThemeColor(theme, "filebar_text_background_clutch"_id, MakeColor(64, 128, 255));

    SetThemeColor(theme, "inner_selection_background"_id, MakeColor(32, 96, 128));
    SetThemeColor(theme, "outer_selection_background"_id, MakeColor(128, 96, 32));
    SetThemeColor(theme, "line_highlight"_id, MakeColor(18, 30, 48));

    SetThemeColor(theme, "command_line_foreground"_id, MakeColor(235, 235, 225));
    SetThemeColor(theme, "command_line_background"_id, MakeColor(6, 10, 16));
    SetThemeColor(theme, "command_line_background_2"_id, MakeColor(12, 20, 32));
    SetThemeColor(theme, "command_line_name"_id, MakeColor(192, 128, 128));
    SetThemeColor(theme, "command_line_option"_id, MakeColor(164, 164, 164));
    SetThemeColor(theme, "command_line_option_selected"_id, MakeColor(192, 128, 128));
    SetThemeColor(theme, "command_line_option_numbers"_id, MakeColor(128, 192, 128));
    SetThemeColor(theme, "command_line_option_numbers_highlighted"_id, MakeColor(128, 255, 128));
    SetThemeColor(theme, "command_line_option_directory"_id, MakeColor(192, 192, 128));
    SetThemeColor(theme, "command_line_option_active"_id, MakeColor(235, 255, 128));
    SetThemeColor(theme, "command_line_option_inactive"_id, MakeColor(192, 192, 192));
}

function void
LoadDefaultLightTheme()
{
    Theme *theme = AddTheme("textit-light"_str);
    SetThemeColor(theme, "text_identifier"_id, MakeColor(12, 12, 12));
    SetThemeColor(theme, "text_keyword"_id, MakeColor(192, 128, 0));
    SetThemeColor(theme, "text_flowcontrol"_id, MakeColor(255, 64, 0));
    SetThemeColor(theme, "text_label"_id, MakeColor(255, 128, 0), TextStyle_Underline|TextStyle_Italic);
    SetThemeColor(theme, "text_preprocessor"_id, MakeColor(192, 64, 192));
    SetThemeColor(theme, "text_string"_id, MakeColor(192, 128, 12));
    SetThemeColor(theme, "text_number"_id, MakeColor(32, 164, 192));
    SetThemeColor(theme, "text_literal"_id, MakeColor(192, 32, 128));
    SetThemeColor(theme, "text_function"_id, MakeColor(64, 100, 164));
    SetThemeColor(theme, "text_macro"_id, MakeColor(12, 12, 12), TextStyle_Bold);
    SetThemeColor(theme, "text_function_macro"_id, MakeColor(64, 100, 164), TextStyle_Bold);
    SetThemeColor(theme, "text_type"_id, MakeColor(12, 128, 64));
    SetThemeColor(theme, "text_comment"_id,      MakeColor(168, 168, 128), TextStyle_Italic);
    SetThemeColor(theme, "text_comment_note"_id, MakeColor(128, 128, 64),  TextStyle_Italic|TextStyle_Bold);
    SetThemeColor(theme, "text_comment_todo"_id, MakeColor(64,  128, 64),  TextStyle_Italic|TextStyle_Bold);
    SetThemeColor(theme, "text_foreground"_id, MakeColor(12, 12, 12));
    SetThemeColor(theme, "text_foreground_dim"_id, MakeColor(96, 96, 96));
    SetThemeColor(theme, "text_foreground_dimmer"_id, MakeColor(148, 148, 148));
    SetThemeColor(theme, "text_foreground_dimmest"_id, MakeColor(196, 196, 196));
    SetThemeColor(theme, "text_background"_id, MakeColor(245, 245, 235));
    SetThemeColor(theme, "text_background_inactive"_id, MakeColor(245, 245, 235));
    SetThemeColor(theme, "text_background_unreachable"_id, MakeColor(225, 225, 225));
    SetThemeColor(theme, "text_background_highlighted"_id, MakeColor(200, 200, 200));
    SetThemeColor(theme, "unrenderable_text_foreground"_id, MakeColor(255, 255, 255));
    SetThemeColor(theme, "unrenderable_text_background"_id, MakeColor(192, 0, 0));

    SetThemeColor(theme, "filebar_text_foreground"_id, MakeColor(0, 0, 0));
    SetThemeColor(theme, "filebar_text_highlighted"_id, MakeColor(192, 128, 0));
    SetThemeColor(theme, "filebar_text_background"_id, MakeColor(92, 192, 92));
    SetThemeColor(theme, "filebar_text_inactive"_id, MakeColor(192, 192, 192));
    SetThemeColor(theme, "filebar_text_background_text_mode"_id, MakeColor(225, 128, 64));
    SetThemeColor(theme, "filebar_text_background_clutch"_id, MakeColor(64, 128, 192));

    SetThemeColor(theme, "inner_selection_background"_id, MakeColor(196, 225, 255));
    SetThemeColor(theme, "outer_selection_background"_id, MakeColor(255, 212, 196));
    SetThemeColor(theme, "line_highlight"_id, MakeColor(245, 235, 215));

    SetThemeColor(theme, "command_line_foreground"_id, MakeColor(12, 12, 12));
    SetThemeColor(theme, "command_line_background"_id, MakeColor(200, 200, 192));
    SetThemeColor(theme, "command_line_name"_id, MakeColor(192, 64, 32));
    SetThemeColor(theme, "command_line_option"_id, MakeColor(24, 24, 24));
    SetThemeColor(theme, "command_line_option_selected"_id, MakeColor(192, 32, 32));
    SetThemeColor(theme, "command_line_option_numbers"_id, MakeColor(32, 128, 32));
    SetThemeColor(theme, "command_line_option_numbers_highlighted"_id, MakeColor(32, 128, 192));
    SetThemeColor(theme, "command_line_option_directory"_id, MakeColor(48, 32, 24));
}

function void
LoadDefaultThemes()
{
    LoadDefaultDarkTheme();
    LoadDefaultLightTheme();
    editor->theme = editor->first_theme;
}
