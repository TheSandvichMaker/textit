#ifndef TEXTIT_HPP
#define TEXTIT_HPP

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "textit_platform.hpp"
#include "textit_intrinsics.hpp"
#include "textit_shared.hpp"
#include "textit_introspection_macros.hpp"
#include "textit_memory.hpp"
#include "textit_sort.hpp"
#include "textit_string.hpp"
#include "textit_global_state.hpp"
#include "textit_math.hpp"
#include "textit_random.hpp"
#include "textit_resources.hpp"
#include "textit_image.hpp"
#include "textit_command.hpp"
#include "textit_theme.hpp"
#include "textit_glyph_cache.hpp"
#include "textit_render.hpp"
#include "textit_tokens.hpp"
#include "textit_language.hpp"
#include "textit_text_storage.hpp"
#include "textit_line_index.hpp"
#include "textit_buffer.hpp"
#include "textit_tokenizer.hpp"
#include "textit_tags.hpp"
#include "textit_project.hpp"
#include "textit_view.hpp"
#include "textit_command_line.hpp"
#include "textit_auto_indent.hpp"

#include "textit_language_cpp.hpp"

#define CURSOR_HASH_SIZE 512

enum InputMode
{
    InputMode_Editor,
};

enum EditMode
{
    EditMode_Command,
    EditMode_Text,
    EditMode_COUNT,
};

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
    Window *first_child;
    Window *last_child;
    Window *next, *prev;

    ViewID view;
};

function void DestroyWindow(Window *window);

#define CoreConfig(_, X)                                           \
    X(_, bool,   visualize_newlines              = false)          \
    X(_, bool,   right_align_visualized_newlines = false)          \
    X(_, bool,   visualize_whitespace            = true)           \
    X(_, bool,   show_line_numbers               = false)          \
    X(_, bool,   incremental_parsing             = false)          \
    X(_, bool,   show_scrollbar                  = false)          \
    X(_, bool,   indent_with_tabs                = true)           \
    X(_, int,    indent_width                    = 4)              \
    X(_, bool,   syntax_highlighting             = true)           \
    X(_, bool,   debug_show_glyph_cache          = false)          \
    X(_, bool,   debug_show_line_index           = false)          \
    X(_, bool,   debug_show_jump_history         = false)          \
    X(_, String, font_name                       = "Consolas"_str) \
    X(_, int,    font_size                       = 15)             \
    X(_, bool,   use_cached_cleartype_blend      = true)           \
    X(_, bool,   auto_line_comments              = true)           \
    X(_, String, theme_name                      = "textit-light"_str)
DeclareIntrospectedStruct(CoreConfig);
GLOBAL_STATE(CoreConfig, core_config);

struct Cursor
{
    Cursor *next;
    int64_t sticky_col;

    int64_t pos;
    Selection selection;
};

function void
SetCursor(Cursor *cursor, int64_t pos, Range inner = {}, Range outer = {})
{
    if (inner.start == 0 &&
        inner.end   == 0)
    {
        inner = MakeRange(pos);
    }
    if (outer.start == 0 &&
        outer.end   == 0)
    {
        outer = inner;
    }
    cursor->pos             = pos;
    cursor->selection.inner = inner;
    cursor->selection.outer = outer;
}

function void
SetSelection(Cursor *cursor, Range inner, Range outer = {})
{
    if (outer.start == 0 &&
        outer.end   == 0)
    {
        outer = inner;
    }
    cursor->selection.inner = inner;
    cursor->selection.outer = outer;
}

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
    union
    {
        CursorHashEntry *next_in_hash;
        CursorHashEntry *next_free;
    };

    CursorHashKey key;
    Cursor *cursor;
};

struct EditorState
{
    GlobalState global_state;

    Arena permanent_arena;
    Arena transient_arena;
    Arena undo_scratch;

    Arena command_arena;

    Theme *first_theme;
    Theme *last_theme;
    Theme *theme;

    InputMode input_mode;
    bool suppress_text_event;
    bool changed_command_line;

    int command_line_count;
    CommandLine *command_lines[64];

    EditMode edit_mode;
    EditMode next_edit_mode;
    BindingMap bindings[EditMode_COUNT];

    PlatformFontHandle fonts[TextStyle_Count];
    V2i font_metrics;
    V2i font_max_glyph_size;

    uint8_t font_name_storage[256];
    StringContainer font_name;

    int font_size;
    PlatformFontQuality font_quality;

    uint8_t search_storage[256];
    StringContainer search;
    StringMatchFlags search_flags;
    bool show_search_highlight;

    IndentRules default_indent_rules;

    Project *first_project;

    uint32_t buffer_count;
    Buffer *buffers[MAX_BUFFER_COUNT];
    BufferID used_buffer_ids[MAX_BUFFER_COUNT];
    BufferID free_buffer_ids[MAX_BUFFER_COUNT];

    Buffer *null_buffer;

    uint32_t view_count;
    View *views[MAX_VIEW_COUNT];
    ViewID used_view_ids[MAX_VIEW_COUNT];
    ViewID free_view_ids[MAX_VIEW_COUNT];

    View *null_view;

    Cursor *override_cursor;
    Cursor *first_free_cursor;
    CursorHashEntry *first_free_cursor_hash_entry;
    CursorHashEntry *cursor_hash[CURSOR_HASH_SIZE];

    LineData *first_free_line_data;
    LineIndexNode *first_free_line_index_node;

    Window root_window;
    Window *active_window;
    Window *first_free_window;

    bool mouse_down;
    int64_t pos_at_mouse;
    Token *token_at_mouse;

    V2i screen_mouse_p;
    V2i text_mouse_p;

    int64_t last_repeat;
    Command *last_movement;
    Command *last_movement_for_change;
    Command *last_change;
    bool clutch;

    struct
    {
        int delay;
        int delay_frame_count;
        int allocated_window_count;

        double line_index_insert_timing;
        double line_index_lookup_timing;
        int64_t line_index_lookup_count;
        int64_t line_index_lookup_recursion_count;

        double buffer_edit_timing;
    } debug;
};
static EditorState *editor;

function void ExecuteCommand(View *view, Command *command);
function Buffer *OpenBufferFromFile(String filename, BufferFlags flags = 0);

function Cursor *CreateCursor(View *view);
function void FreeCursor(Cursor *cursor);
function Cursor **GetCursorSlot(ViewID view, BufferID buffer, bool make_if_missing);
function Cursor *GetCursor(ViewID view, BufferID buffer);
function Cursor *GetCursor(View *view, Buffer *buffer = nullptr);
function Cursor *IterateCursors(ViewID view, BufferID buffer);
function Cursor *IterateCursors(View *view);

function void SetEditorFont(String name, int size, PlatformFontQuality quality);

struct OpenBufferFromFileJobArgs
{
    String name;
    BufferFlags flags;
};
function PLATFORM_JOB(OpenBufferFromFileJob);

struct BufferIterator
{
    size_t index;
    Buffer *buffer;
};

function BufferIterator
IterateBuffers(void)
{
    BufferIterator result = {};
    result.index  = 1;
    result.buffer = GetBuffer(editor->used_buffer_ids[result.index]);
    return result;
}

function bool
IsValid(BufferIterator *iter)
{
    return (iter->index < editor->buffer_count);
}

function void
Next(BufferIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor->buffer_count)
    {
        iter->buffer = GetBuffer(editor->used_buffer_ids[iter->index]);
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
    result.index = 1;
    result.view  = GetView(editor->used_view_ids[result.index]);
    return result;
}

function bool
IsValid(ViewIterator *iter)
{
    return (iter->index < editor->view_count);
}

function void
Next(ViewIterator *iter)
{
    iter->index += 1;
    if (iter->index < editor->view_count)
    {
        iter->view = GetView(editor->used_view_ids[iter->index]);
    }
}

function View *
GetActiveView(void)
{
    return GetView(editor->active_window->view);
}

function Buffer *
GetActiveBuffer(void)
{
    return GetBuffer(GetView(editor->active_window->view)->buffer);
}

function void SplitWindow(Window *window, WindowSplitKind split);
function void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
