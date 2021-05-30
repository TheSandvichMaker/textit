#ifndef TEXTIT_THEME_HPP
#define TEXTIT_THEME_HPP

struct ThemeColor
{
    String name;
    Color color;
};

#define MAX_THEME_COLORS 256
struct Theme
{
    size_t color_count;
    ThemeColor colors[MAX_THEME_COLORS];
};

static inline void LoadDefaultTheme();
static inline void SetThemeColor(String name, Color color);
static inline Color GetThemeColor(String name);

#endif /* TEXTIT_THEME_HPP */
