#ifndef TEXTIT_THEME_HPP
#define TEXTIT_THEME_HPP

struct ThemeColor
{
    StringID       key;
    Color          color;
    TextStyleFlags style;
};

#define MAX_THEME_COLORS 256
struct Theme
{
    Theme *next;
    String name;
    ThemeColor map[MAX_THEME_COLORS];
};

function void LoadDefaultTheme();
function Color GetThemeColor(StringID key);
function TextStyleFlags GetThemeStyle(StringID key);

#endif /* TEXTIT_THEME_HPP */
