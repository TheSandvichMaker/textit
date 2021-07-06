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
#include "textit_text_storage.hpp"
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

function StringMap *PushStringMap(Arena *arena, size_t size);
function void *StringMapFind(StringMap *map, String string);
function void StringMapInsert(StringMap *map, String string, void *data);

enum WindowSplitKind
{
    WindowSplit_Leaf,
    WindowSplit_Horz,
    WindowSplit_Vert,
};

struct Window
{
    WindowSplitKind split;

    Window *parent;
    Window *left, *right;

    ViewID view;
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

function Range GetEditRange(Cursor *cursor);

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

    TextStorage default_register;
    TextStorage registers[26];

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

    Window root_window;
    Window *active_window;

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

function void ExecuteCommand(View *view, Command *command);

function Cursor *GetCursor(ViewID view, BufferID buffer);
function Cursor *GetCursor(View *view, Buffer *buffer = nullptr);

struct BufferIterator
{
    size_t index;
    Buffer *buffer;
};

function BufferIterator
IterateBuffers(void)
{
    BufferIterator result = {};
    result.buffer = GetBuffer(editor_state->used_buffer_ids[0]);
    return result;
}

function bool
IsValid(BufferIterator *iter)
{
    return (iter->index < editor_state->buffer_count);
}

function void
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

function ViewIterator
IterateViews(void)
{
    ViewIterator result = {};
    result.view = GetView(editor_state->used_view_ids[0]);
    return result;
}

function bool
IsValid(ViewIterator *iter)
{
    return (iter->index < editor_state->view_count);
}

function void
Next(ViewIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor_state->view_count)
    {
        iter->view = GetView(editor_state->used_view_ids[iter->index]);
    }
}

function View *
CurrentView(EditorState *editor)
{
    return GetView(editor->active_window->view);
}

function Buffer *
CurrentBuffer(EditorState *editor)
{
    View *view = GetView(editor->active_window->view);
    Buffer *result = GetBuffer(view->buffer);
    return result;
}

function void SplitWindow(Window *window, WindowSplitKind split);
function void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
