#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_buffer.cpp"
#include "textit_view.cpp"
#include "textit_command.cpp"
#include "textit_base_commands.cpp"
#include "textit_draw.cpp"

static inline Buffer *
OpenNewBuffer(String buffer_name, BufferFlags flags = 0)
{
    EditorState *editor = editor_state;

    BufferID id = {};
    if (editor->buffer_count < MAX_BUFFER_COUNT)
    {
        id = editor_state->free_buffer_ids[MAX_BUFFER_COUNT - editor_state->buffer_count - 1];
        id.generation += 1;

        editor_state->used_buffer_ids[editor_state->buffer_count] = id;
        editor_state->buffer_count += 1;
    }

    Buffer *result = BootstrapPushStruct(Buffer, arena);
    result->id = id;
    result->flags = flags;
    result->name = PushString(&result->arena, buffer_name);
    result->undo.at = &result->undo.root;

    editor->buffers[result->id.index] = result;

    return result;
}

static inline Buffer *
OpenBufferFromFile(String filename)
{
    Buffer *result = OpenNewBuffer(filename);
    size_t file_size = platform->ReadFileInto(sizeof(result->text), result->text, filename);
    result->count = (int64_t)file_size;
    result->line_end = GuessLineEndKind(MakeString(result->count, (uint8_t *)result->text));
    return result;
}

static inline Buffer *
GetBuffer(BufferID id)
{
    Buffer *result = editor_state->buffers[0];
    if (id.index > 0 && id.index < MAX_BUFFER_COUNT)
    {
        Buffer *buffer = editor_state->buffers[id.index];
        if (buffer && buffer->id == id)
        {
            result = buffer;
        }
    }
    return result;
}

static inline void
DestroyBuffer(BufferID id)
{
    Buffer *buffer = GetBuffer(id);
    if (buffer->flags & Buffer_Indestructible)
    {
        return;
    }

    size_t used_id_index = 0;
    for (size_t i = 0; i < editor_state->buffer_count; i += 1)
    {
        BufferID test_id = editor_state->used_buffer_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor_state->free_buffer_ids[MAX_BUFFER_COUNT - editor_state->buffer_count] = id;
    editor_state->used_buffer_ids[used_id_index] = editor_state->used_buffer_ids[--editor_state->buffer_count];

    Release(&buffer->arena);
    editor_state->buffers[id.index] = nullptr;
}

static inline View *
OpenNewView(BufferID buffer)
{
    EditorState *editor = editor_state;

    ViewID id = {};
    if (editor->view_count < MAX_VIEW_COUNT)
    {
        id = editor_state->free_view_ids[MAX_VIEW_COUNT - editor_state->view_count - 1];
        id.generation += 1;

        editor_state->used_view_ids[editor_state->view_count] = id;
        editor_state->view_count += 1;
    }

    View *result = BootstrapPushStruct(View, arena);
    result->id =  id;
    result->buffer = buffer;

    editor->views[result->id.index] = result;

    return result;
}

static inline View *
GetView(ViewID id)
{
    View *result = editor_state->views[0];
    if (id.index > 0 && id.index < MAX_VIEW_COUNT)
    {
        View *view = editor_state->views[id.index];
        if (view && view->id == id)
        {
            result = view;
        }
    }
    return result;
}

static inline void
DestroyView(ViewID id)
{
    View *view = GetView(id);

    size_t used_id_index = 0;
    for (size_t i = 0; i < editor_state->view_count; i += 1)
    {
        ViewID test_id = editor_state->used_view_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor_state->free_view_ids[MAX_VIEW_COUNT - editor_state->view_count] = id;
    editor_state->used_view_ids[used_id_index] = editor_state->used_view_ids[--editor_state->view_count];

    Release(&view->arena);
    editor_state->views[id.index] = nullptr;
}

static inline ViewID
DuplicateView(ViewID id)
{
    ViewID result = ViewID::Null();

    View *view = GetView(id);
    if (view != editor_state->null_view)
    {
        View *new_view = OpenNewView(view->buffer);
        new_view->cursor = view->cursor;
        new_view->scroll_at = view->scroll_at;
        result = new_view->id;
    }

    return result;
}

static inline void
SplitWindow(Window *window, WindowSplitKind split)
{
    Assert(window->is_leaf);

    ViewID view = window->view;
    window->is_leaf = false;
    window->split = split;

    Window *a = window->a = PushStruct(&editor_state->transient_arena, Window);
    a->is_leaf = true;
    a->view = view;

    Window *b = window->b = PushStruct(&editor_state->transient_arena, Window);
    b->is_leaf = true;
    b->view = DuplicateView(view);
}

static inline void
RecalculateViewBounds(Window *window, Rect2i rect)
{
    if (window->is_leaf)
    {
        View *view = GetView(window->view);
        view->viewport = rect;
    }
    else
    {
        Rect2i a_rect, b_rect;
        if (window->split == WindowSplit_Horz)
        {
            int64_t split = GetHeight(rect) / 2;
            SplitRect2iHorizontal(rect, split, &a_rect, &b_rect);
        }
        else
        {
            Assert(window->split == WindowSplit_Vert);
            int64_t split = GetWidth(rect) / 2;
            SplitRect2iVertical(rect, split, &a_rect, &b_rect);
        }
        RecalculateViewBounds(window->a, a_rect);
        RecalculateViewBounds(window->b, b_rect);
    }
}


static inline void
DrawWindows(Window *window)
{
    if (window->is_leaf)
    {
        View *view = GetView(window->view);

        int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
        int64_t top = view->scroll_at;
        int64_t bot = view->scroll_at + viewport_height;
        if (view->cursor.y < top)
        {
            view->scroll_at += view->cursor.y - top;
        }
        if (view->cursor.y > bot)
        {
            view->scroll_at += view->cursor.y - bot;
        }

        DrawView(view);
    }
    else
    {
        DrawWindows(window->a);
        DrawWindows(window->b);
    }
}

static inline void
LoadDefaultBindings()
{
    BindingMap *command = &editor_state->bindings[EditMode_Command];
    command->text_command = nullptr;
    command->map[PlatformInputCode_Left].regular  = FindCommand("MoveLeft"_str);
    command->map[PlatformInputCode_Right].regular = FindCommand("MoveRight"_str);
    command->map[PlatformInputCode_Left].ctrl     = FindCommand("MoveLeftIdentifier"_str);
    command->map[PlatformInputCode_Right].ctrl    = FindCommand("MoveRightIdentifier"_str);
    command->map[PlatformInputCode_Up].regular    = FindCommand("MoveUp"_str);
    command->map[PlatformInputCode_Down].regular  = FindCommand("MoveDown"_str);
    command->map[PlatformInputCode_Up].ctrl       = FindCommand("MoveUp"_str);
    command->map[PlatformInputCode_Down].ctrl     = FindCommand("MoveDown"_str);
    command->map['H'].regular                     = FindCommand("MoveLeft"_str);
    command->map['J'].regular                     = FindCommand("MoveDown"_str);
    command->map['K'].regular                     = FindCommand("MoveUp"_str);
    command->map['L'].regular                     = FindCommand("MoveRight"_str);
    command->map['W'].regular                     = FindCommand("MoveRightIdentifier"_str);
    command->map['B'].regular                     = FindCommand("MoveLeftIdentifier"_str);
    command->map['I'].regular                     = FindCommand("EnterTextMode"_str);
    command->map['X'].regular                     = FindCommand("DeleteChar"_str);
    command->map['U'].regular                     = FindCommand("UndoOnce"_str);
    command->map['R'].ctrl                        = FindCommand("RedoOnce"_str);
    command->map['V'].ctrl                        = FindCommand("SplitWindowVertical"_str);

    BindingMap *text = &editor_state->bindings[EditMode_Text];
    text->text_command = FindCommand("WriteText"_str);
    text->map[PlatformInputCode_Left].regular     = FindCommand("MoveLeft"_str);
    text->map[PlatformInputCode_Right].regular    = FindCommand("MoveRight"_str);
    text->map[PlatformInputCode_Left].ctrl        = FindCommand("MoveLeftIdentifier"_str);
    text->map[PlatformInputCode_Right].ctrl       = FindCommand("MoveRightIdentifier"_str);
    text->map[PlatformInputCode_Up].regular       = FindCommand("MoveUp"_str);
    text->map[PlatformInputCode_Down].regular     = FindCommand("MoveDown"_str);
    text->map[PlatformInputCode_Up].ctrl          = FindCommand("MoveUp"_str);
    text->map[PlatformInputCode_Down].ctrl        = FindCommand("MoveDown"_str);
    text->map[PlatformInputCode_Escape].regular   = FindCommand("EnterCommandMode"_str);
    text->map[PlatformInputCode_Back].regular     = FindCommand("BackspaceChar"_str);
    text->map[PlatformInputCode_Back].ctrl        = FindCommand("BackspaceWord"_str);
    text->map[PlatformInputCode_Delete].regular   = FindCommand("DeleteChar"_str);
    text->map[PlatformInputCode_Delete].ctrl      = FindCommand("DeleteWord"_str);
}

static inline StringMap *
PushStringMap(Arena *arena, size_t size)
{
    StringMap *result = PushStruct(arena, StringMap);
    result->arena = arena;
    result->size = size;
    result->nodes = PushArray(arena, result->size, StringMapNode *);
    return result;
}

static inline void *
StringMapFind(StringMap *map, String string)
{
    void *result = nullptr;
    uint64_t hash = HashString(string).u64[0];
    uint64_t slot = hash % map->size;
    for (StringMapNode *test_node = map->nodes[slot]; test_node; test_node = test_node->next)
    {
        if (test_node->hash == hash)
        {
            if (AreEqual(test_node->string, string))
            {
                result = test_node->data;
                break;
            }
        }
    }
    return result;
}

static inline void
StringMapInsert(StringMap *map, String string, void *data)
{
    uint64_t hash = HashString(string).u64[0];
    uint64_t slot = hash % map->size;
    StringMapNode *node = nullptr;
    for (StringMapNode *test_node = map->nodes[slot]; test_node; test_node = test_node->next)
    {
        if (test_node->hash == hash)
        {
            if (AreEqual(test_node->string, string))
            {
                node = test_node;
                break;
            }
        }
    }
    if (!node)
    {
        node = PushStruct(map->arena, StringMapNode);

        node->hash = hash;
        node->string = PushString(map->arena, string);

        node->next = map->nodes[slot];
        map->nodes[slot] = node;
    }
    node->data = data;
}

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
    SetThemeColor("text_foreground"_str, MakeColor(255, 255, 255));
    SetThemeColor("text_background"_str, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_foreground"_str, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_str, MakeColor(192, 255, 128));
    SetThemeColor("filebar_text_background_text_mode"_str, MakeColor(255, 192, 128));
    SetThemeColor("unrenderable_text_foreground"_str, MakeColor(255, 255, 255));
    SetThemeColor("unrenderable_text_background"_str, MakeColor(192, 0, 0));
}

// Ryan's text controls example: https://hatebin.com/ovcwtpsfmj

static inline void
SetDebugDelay(int milliseconds, int frame_count)
{
    editor_state->debug_delay = milliseconds;
    editor_state->debug_delay_frame_count = frame_count;
}

static Font
MakeFont(Bitmap bitmap, int32_t glyph_w, int32_t glyph_h)
{
    Font font = {};

    font.w = bitmap.w;
    font.h = bitmap.h;
    font.pitch = bitmap.pitch;
    font.glyph_w = glyph_w;
    font.glyph_h = glyph_h;

    // you probably got it wrong if there's slop
    Assert(font.w % glyph_w == 0);
    Assert(font.h % glyph_h == 0);

    font.glyphs_per_row = font.w / glyph_w;
    font.glyphs_per_col = font.h / glyph_h;
    font.glyph_count = font.glyphs_per_col*font.glyphs_per_row;

    font.data = bitmap.data;

    return font;
}

static Font
LoadFontFromDisk(Arena *arena, String filename, int glyph_w, int glyph_h)
{
    Font result = {};

    String file = platform->ReadFile(arena, filename);
    if (!file.size)
    {
        platform->ReportError(PlatformError_Nonfatal,
                              "Could not open file '%.*s' while loading font.", StringExpand(filename));
        return result;
    }

    Bitmap bitmap = ParseBitmap(file);
    if (!bitmap.data)
    {
        platform->ReportError(PlatformError_Nonfatal,
                              "Failed to parse bitmap '%.*s' while loading font.", StringExpand(filename));
        return result;
    }

    result = MakeFont(bitmap, glyph_w, glyph_h);
    return result;
}

static inline Bitmap
GetGlyphBitmap(Font *font, Glyph glyph)
{
    Rect2i rect = GetGlyphRect(font, glyph);
    Bitmap result = MakeBitmapView(&font->bitmap, rect);
    return result;
}

static inline void
HandleViewEvents(View *view)
{
    Buffer *buffer = GetBuffer(view);

    for (PlatformEvent *event = nullptr;
         platform->NextEvent(&event, PlatformEventFilter_ANY);
         )
    {
        BindingMap *bindings = &editor_state->bindings[editor_state->edit_mode];

        BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);
        if ((event->type == PlatformEvent_Text) && bindings->text_command)
        {
            String text = MakeString(event->text_length, event->text);
            bindings->text_command->text_proc(editor_state, text);
        }
        else if (MatchFilter(event->type, PlatformEventFilter_Input) &&
                 event->pressed)
        {
            bool ctrl_down = event->ctrl_down;
            bool shift_down = event->shift_down;
            bool ctrl_shift_down = ctrl_down && shift_down;

            Binding *binding = &bindings->map[event->input_code];
            Command *command = binding->regular;
            if (ctrl_shift_down)
            {
                command = binding->ctrl_shift;
            }
            else if (ctrl_down)
            {
                command = binding->ctrl;
            }
            else if (shift_down)
            {
                command = binding->shift;
            }

            if (command)
            {
                command->proc(editor_state);
            }
        }
    }
}

void
AppUpdateAndRender(Platform *platform_)
{
    platform = platform_;

    editor_state = (EditorState *)platform->persistent_app_data;
    if (!platform->app_initialized)
    {
        editor_state = BootstrapPushStruct(EditorState, permanent_arena);
        platform->persistent_app_data = editor_state;
    }

    if (platform->exe_reloaded)
    {
        RestoreGlobalState(&editor_state->global_state, &editor_state->transient_arena);
    }

    if (!platform->app_initialized)
    {
        editor_state->font = LoadFontFromDisk(&editor_state->transient_arena, StringLiteral("font8x16.bmp"), 8, 16);
        InitializeRenderState(&editor_state->transient_arena, &platform->backbuffer, &editor_state->font);

        LoadDefaultTheme();
        LoadDefaultBindings();

        for (int16_t i = 0; i < MAX_BUFFER_COUNT; i += 1)
        {
            editor_state->free_buffer_ids[i].index = MAX_BUFFER_COUNT - i - 1;
        }

        for (int16_t i = 0; i < MAX_VIEW_COUNT; i += 1)
        {
            editor_state->free_view_ids[i].index = MAX_VIEW_COUNT - i - 1;
        }

        editor_state->null_buffer = OpenNewBuffer("null"_str, Buffer_Indestructible|Buffer_ReadOnly);
        editor_state->message_buffer = OpenNewBuffer("messages"_str, Buffer_Indestructible|Buffer_ReadOnly);

        editor_state->null_view = OpenNewView(BufferID::Null());

        Buffer *test_buffer = OpenBufferFromFile("test_file.txt"_str);
        View *view = OpenNewView(test_buffer->id);

        editor_state->active_view = view->id;

        editor_state->root_window.is_leaf = true;
        editor_state->root_window.view = editor_state->active_view;

        platform->PushTickEvent();
        platform->app_initialized = true;
    }

    editor_state->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor_state->text_mouse_p = editor_state->screen_mouse_p / GlyphDim(&editor_state->font);

    HandleViewEvents(GetView(editor_state->active_view));
    RecalculateViewBounds(&editor_state->root_window, render_state->viewport);

    BeginRender();
    DrawWindows(&editor_state->root_window);
    EndRender();
    
    Buffer *buffer = CurrentBuffer(editor_state);
    if ((editor_state->next_edit_mode == EditMode_Text) &&
        (buffer->flags & Buffer_ReadOnly))
    {
        editor_state->next_edit_mode = EditMode_Command;
    }
    editor_state->edit_mode = editor_state->next_edit_mode;

    if (editor_state->debug_delay_frame_count > 0)
    {
        platform->SleepThread(editor_state->debug_delay);
        editor_state->debug_delay_frame_count -= 1;
    }
}
