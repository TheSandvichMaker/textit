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
#include "textit_window.hpp"
#include "textit_cursor.hpp"
#include "textit_draw.hpp"
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

#define CoreConfig(_, X)                                               \
    X(_, bool,   visualize_newlines              = false)              \
    X(_, bool,   right_align_visualized_newlines = false)              \
    X(_, bool,   visualize_whitespace            = false)              \
    X(_, bool,   show_line_numbers               = false)              \
    X(_, bool,   incremental_parsing             = false)              \
    X(_, bool,   show_scrollbar                  = false)              \
    X(_, bool,   indent_with_tabs                = true)               \
    X(_, int,    indent_width                    = 4)                  \
    X(_, bool,   syntax_highlighting             = true)               \
    X(_, bool,   debug_show_glyph_cache          = false)              \
    X(_, bool,   debug_show_line_index           = false)              \
    X(_, bool,   debug_show_jump_history         = false)              \
    X(_, String, font_name                       = "Consolas"_str)     \
    X(_, int,    font_size                       = 15)                 \
    X(_, bool,   use_cached_cleartype_blend      = true)               \
    X(_, bool,   auto_line_comments              = true)               \
    X(_, String, theme_name                      = "textit-light"_str) \
    X(_, int,    view_autoscroll_margin          = 1)
DeclareIntrospectedStruct(CoreConfig);
GLOBAL_STATE(CoreConfig, core_config);

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

    Project *active_project;
    Project *first_free_project;
    Project project_sentinel;

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

function void SetEditorFont(String name, int size, PlatformFontQuality quality);

function void SplitWindow(Window *window, WindowSplitKind split);
function void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
