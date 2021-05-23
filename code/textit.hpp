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
#include "textit_resources.hpp"
#include "textit_image.hpp"
#include "textit_command.hpp"
#include "textit_render.hpp"
#include "textit_buffer.hpp"
#include "textit_view.hpp"

enum EditMode
{
    EditMode_Command,
    EditMode_Text,
    EditMode_COUNT,
};

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

static inline StringMap *PushStringMap(Arena *arena, size_t size);
static inline void *StringMapFind(StringMap *map, String string);
static inline void StringMapInsert(StringMap *map, String string, void *data);

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

struct EditorState
{
    GlobalState global_state;

    Arena permanent_arena;
    Arena transient_arena;

    Theme theme;

    EditMode edit_mode;
    EditMode next_edit_mode;
    BindingMap bindings[EditMode_COUNT];

    Font font;

    uint32_t buffer_count;
    Buffer *buffers[MAX_BUFFER_COUNT];
    BufferID used_buffer_ids[MAX_BUFFER_COUNT];
    BufferID free_buffer_ids[MAX_BUFFER_COUNT];

    Buffer *null_buffer;
    Buffer *message_buffer;

    uint32_t view_count;
    View *views[MAX_VIEW_COUNT];
    ViewID used_view_ids[MAX_VIEW_COUNT];
    ViewID free_view_ids[MAX_VIEW_COUNT];

    View *open_view;

    V2i screen_mouse_p;
    V2i text_mouse_p;

    uint64_t enter_text_mode_undo_ordinal;

    int debug_delay;
    int debug_delay_frame_count;
};
static EditorState *editor_state;

struct BufferIterator
{
    size_t index;
    Buffer *buffer;
};

static inline BufferIterator
IterateBuffers(void)
{
    BufferIterator result = {};
    result.buffer = GetBuffer(editor_state->used_buffer_ids[0]);
    return result;
}

static inline bool
IsValid(BufferIterator *iter)
{
    return (iter->index < editor_state->buffer_count);
}

static inline void
Next(BufferIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor_state->buffer_count)
    {
        iter->buffer = GetBuffer(editor_state->used_buffer_ids[iter->index]);
    }
}

static inline View *
CurrentView(EditorState *editor)
{
    return editor->open_view;
}

static inline Buffer *
CurrentBuffer(EditorState *editor)
{
    Buffer *result = editor->null_buffer;
    if (editor->open_view)
    {
        result = GetBuffer(editor->open_view->buffer);
    }
    return result;
}

static inline void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
