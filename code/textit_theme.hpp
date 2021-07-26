#ifndef TEXTIT_THEME_HPP
#define TEXTIT_THEME_HPP

struct ThemeColor
{
    StringID key;
    Color color;
};

#define MAX_THEME_COLORS 256
struct Theme
{
    ThemeColor map[MAX_THEME_COLORS];
};

function void LoadDefaultTheme();
function void SetThemeColor(StringID key, Color color);
function Color GetThemeColor(StringID key);

#endif /* TEXTIT_THEME_HPP */
