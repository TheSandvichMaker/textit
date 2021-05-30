static inline void
SetThemeColor(String name, Color color)
{
    Theme *theme = &editor_state->theme;
    if (theme->color_count < MAX_THEME_COLORS)
    {
        ThemeColor *theme_color = &theme->colors[theme->color_count++];
        theme_color->name = name;
        theme_color->color = color;
    }
    else
    {
        INVALID_CODE_PATH;
    }
}

static inline Color
GetThemeColor(String name)
{
    Color result = MakeColor(255, 0, 255);

    Theme *theme = &editor_state->theme;
    for (size_t i = 0; i < theme->color_count; ++i)
    {
        ThemeColor *theme_color = &theme->colors[i];
        if (AreEqual(name, theme_color->name))
        {
            result = theme_color->color;
        }
    }

    return result;
}

static inline void
LoadDefaultTheme()
{
    SetThemeColor("text_identifier"_str, MakeColor(255, 255, 255));
    SetThemeColor("text_keyword"_str, MakeColor(255, 192, 0));
    SetThemeColor("text_flowcontrol"_str, MakeColor(255, 128, 0));
    SetThemeColor("text_preprocessor"_str, MakeColor(255, 192, 255));
    SetThemeColor("text_string"_str, MakeColor(255, 192, 64));
    SetThemeColor("text_number"_str, MakeColor(64, 192, 255));
    SetThemeColor("text_literal"_str, MakeColor(64, 128, 255));
    SetThemeColor("text_line_comment"_str, MakeColor(0, 192, 0));
    SetThemeColor("text_type"_str, MakeColor(0, 192, 255));
    SetThemeColor("text_comment"_str, MakeColor(0, 192, 0));
    SetThemeColor("text_foreground"_str, MakeColor(255, 255, 255));
    SetThemeColor("text_foreground_dim"_str, MakeColor(192, 192, 192));
    SetThemeColor("text_background"_str, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_foreground"_str, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_str, MakeColor(192, 255, 128));
    SetThemeColor("filebar_text_background_text_mode"_str, MakeColor(255, 192, 128));
    SetThemeColor("unrenderable_text_foreground"_str, MakeColor(255, 255, 255));
    SetThemeColor("unrenderable_text_background"_str, MakeColor(192, 0, 0));
    SetThemeColor("selection_background"_str, MakeColor(64, 128, 255));
}
