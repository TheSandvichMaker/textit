#ifndef TEXTIT_HPP
#define TEXTIT_HPP

#include <stdarg.h>
#include <stdio.h>

#include "textit_platform.hpp"
#include "textit_intrinsics.hpp"
#include "textit_shared.hpp"
#include "textit_memory.hpp"
#include "textit_string.hpp"
#include "textit_global_state.hpp"
#include "textit_math.hpp"
#include "textit_random.hpp"
#include "textit_image.hpp"
#include "textit_render.hpp"
#include "textit_buffer.hpp"
#include "textit_view.hpp"

struct StringMapNode
{
    StringMapNode *next;
    uint64_t hash;
    String string;
    void *data;
};

struct StringMap
{
    Arena *arena;
    size_t size;
    StringMapNode **nodes;
};

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

static inline StringMap *PushStringMap(Arena *arena, size_t size);
static inline void *StringMapFind(StringMap *map, String string);
static inline void StringMapInsert(StringMap *map, String string, void *data);

struct EditorState
{
    GlobalState global_state;

    Arena permanent_arena;
    Arena transient_arena;

    Theme theme;

    Font font;

    Buffer *open_buffer;
    View *open_view;

    V2i screen_mouse_p;
    V2i text_mouse_p;

    int debug_delay;
    int debug_delay_frame_count;
};
static EditorState *editor_state;

static inline void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
