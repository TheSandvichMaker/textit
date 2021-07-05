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
#include "textit_theme.hpp"
#include "textit_render.hpp"
#include "textit_tokens.hpp"
#include "textit_buffer.hpp"
#include "textit_tokenizer.hpp"
#include "textit_view.hpp"

#define CURSOR_HASH_SIZE 512

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

enum WindowSplitKind
{
    WindowSplit_Horz,
    WindowSplit_Vert,
};

struct Window
{
    bool is_leaf;
    WindowSplitKind split;
    union
    {
        struct
        {
            Window *a, *b;
        };
        ViewID view;
    };
};

struct CoreConfig
{
    bool visualize_newlines = false;
};
GLOBAL_STATE(CoreConfig, core_config);

struct Cursor
{
    Cursor *next;
    int64_t sticky_col;
    int64_t pos;
    int64_t mark;
};

function Range GetEditRange(View *view, Buffer *buffer, Cursor *cursor);

struct CursorHashKey
{
    union
    {
        struct
        {
            ViewID view;
            BufferID buffer;
        };

        uint64_t value;
    };
};

struct CursorHashEntry
{
    CursorHashEntry *next_in_hash;
    CursorHashKey key;
    Cursor cursor;
};

struct EditorState
{
    GlobalState global_state;

    Arena permanent_arena;
    Arena transient_arena;
    Arena undo_scratch;

    Theme theme;

    EditMode edit_mode;
    EditMode next_edit_mode;
    BindingMap bindings[EditMode_COUNT];

    Font font;

    LanguageSpec *cpp_spec;

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

    View *null_view;

    CursorHashEntry *first_free_cursor_hash_entry;
    CursorHashEntry *cursor_hash[CURSOR_HASH_SIZE];

    ViewID active_view;
    Window root_window;

    V2i screen_mouse_p;
    V2i text_mouse_p;

    Command *last_movement;
    Command *last_movement_for_change;
    Command *last_change;
    bool clutch;

    uint64_t enter_text_mode_undo_ordinal;

    TokenBlock *first_free_token_block;

    int debug_delay;
    int debug_delay_frame_count;
};
static EditorState *editor_state;

static inline void ExecuteCommand(View *view, Command *command);

function Cursor *GetCursor(ViewID view, BufferID buffer);
function Cursor *GetCursor(View *view);

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

struct ViewIterator
{
    size_t index;
    View *view;
};

static inline ViewIterator
IterateViews(void)
{
    ViewIterator result = {};
    result.view = GetView(editor_state->used_view_ids[0]);
    return result;
}

static inline bool
IsValid(ViewIterator *iter)
{
    return (iter->index < editor_state->view_count);
}

static inline void
Next(ViewIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor_state->view_count)
    {
        iter->view = GetView(editor_state->used_view_ids[iter->index]);
    }
}

static inline View *
CurrentView(EditorState *editor)
{
    return GetView(editor->active_view);
}

static inline Buffer *
CurrentBuffer(EditorState *editor)
{
    View *view = GetView(editor->active_view);
    Buffer *result = GetBuffer(view->buffer);
    return result;
}

static inline void SplitWindow(Window *window, WindowSplitKind split);
static inline void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
