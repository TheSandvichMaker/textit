#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_text_storage.cpp"
#include "textit_buffer.cpp"
#include "textit_tokenizer.cpp"
#include "textit_view.cpp"
#include "textit_command.cpp"
#include "textit_theme.cpp"
#include "textit_base_commands.cpp"
#include "textit_draw.cpp"

function Cursor *
GetCursor(ViewID view, BufferID buffer)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value);
    uint64_t slot = hash % ArrayCount(editor_state->cursor_hash);

    CursorHashEntry *entry = editor_state->cursor_hash[slot];

    while (entry &&
           (entry->key.value != key.value))
    {
        entry = entry->next_in_hash;
    }

    if (!entry)
    {
        if (!editor_state->first_free_cursor_hash_entry)
        {
            editor_state->first_free_cursor_hash_entry = PushStructNoClear(&editor_state->transient_arena, CursorHashEntry);
            editor_state->first_free_cursor_hash_entry->next_in_hash = nullptr;
        }
        entry = editor_state->first_free_cursor_hash_entry;
        editor_state->first_free_cursor_hash_entry = entry->next_in_hash;

        entry->next_in_hash = editor_state->cursor_hash[slot];
        editor_state->cursor_hash[slot] = entry;

        entry->key = key;
        ZeroStruct(&entry->cursor);
    }

    return &entry->cursor;
}

function Cursor *
GetCursor(View *view, Buffer *buffer)
{
    return GetCursor(view->id, (buffer ? buffer->id : view->buffer));
}

function Range
GetEditRange(Cursor *cursor)
{
    Range result = MakeSanitaryRange(cursor->pos, cursor->mark);
    return result;
}

function Buffer *
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

    AllocateTextStorage(result, TEXTIT_BUFFER_SIZE);

    result->tokens = &result->tokens_[0];
    result->prev_tokens = &result->tokens_[1];

    editor->buffers[result->id.index] = result;

    return result;
}

function Buffer *
OpenBufferFromFile(String filename)
{
    Buffer *result = OpenNewBuffer(filename);
    size_t file_size = platform->GetFileSize(filename);
    EnsureSpace(result, file_size);
    if (platform->ReadFileInto(TEXTIT_BUFFER_SIZE, result->text, filename) != file_size)
    {
        INVALID_CODE_PATH;
    }
    result->count = (int64_t)file_size;
    result->line_end = GuessLineEndKind(MakeString(result->count, (uint8_t *)result->text));
    result->language = editor_state->cpp_spec;

    if (result->count < BUFFER_ASYNC_THRESHOLD)
    {
        TokenizeBuffer(result);
    }
    else
    {
        platform->AddJob(platform->low_priority_queue, result, TokenizeBufferJob);
    }

    return result;
}

function Buffer *
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

function void
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

function View *
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

function View *
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

function void
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

function ViewID
DuplicateView(ViewID id)
{
    ViewID result = ViewID::Null();

    View *view = GetView(id);
    if (view != editor_state->null_view)
    {
        View *new_view = OpenNewView(view->buffer);

        Cursor *cursor = GetCursor(view);
        SetCursor(new_view, cursor->pos, cursor->mark);

        new_view->scroll_at = view->scroll_at;
        result = new_view->id;
    }

    return result;
}

function void
SplitWindow(Window *window, WindowSplitKind split)
{
    Assert(window->split == WindowSplit_Leaf);

    ViewID view = window->view;
    window->split = split;

    Window *left = window->left = PushStruct(&editor_state->transient_arena, Window);
    left->parent = window;
    left->view = view;

    Window *right = window->right = PushStruct(&editor_state->transient_arena, Window);
    right->parent = window;
    right->view = DuplicateView(view);

    editor_state->active_window = right;
}

function void
RecalculateViewBounds(Window *window, Rect2i rect)
{
    if (window->split == WindowSplit_Leaf)
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
            SplitRect2iHorizontal(rect, split, &b_rect, &a_rect);
        }
        else
        {
            Assert(window->split == WindowSplit_Vert);
            int64_t split = GetWidth(rect) / 2;
            SplitRect2iVertical(rect, split, &a_rect, &b_rect);
        }
        RecalculateViewBounds(window->left, a_rect);
        RecalculateViewBounds(window->right, b_rect);
    }
}

function void
DrawWindows(Window *window)
{
    if (window->split == WindowSplit_Leaf)
    {
        View *view = GetView(window->view);

        int64_t estimated_viewport_height = view->actual_viewport_line_height;
        if (!estimated_viewport_height)
        {
            estimated_viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
        }

        {
            int64_t top = view->scroll_at;
            int64_t bot = view->scroll_at + estimated_viewport_height;

            Cursor *cursor = GetCursor(view);
            BufferLocation loc = CalculateBufferLocationFromPos(GetBuffer(view), cursor->pos);

            if (loc.line < top)
            {
                view->scroll_at += loc.line - top;
            }
            if (loc.line > bot)
            {
                view->scroll_at += loc.line - bot;
            }
        }

        bool is_active_window = (window == editor_state->active_window);

        view->actual_viewport_line_height = DrawView(view, is_active_window);

        {
            int64_t viewport_height = view->actual_viewport_line_height;
            if (viewport_height != estimated_viewport_height)
            {
                int64_t top = view->scroll_at;
                int64_t bot = view->scroll_at + viewport_height;

                Cursor *cursor = GetCursor(view);
                BufferLocation loc = CalculateBufferLocationFromPos(GetBuffer(view), cursor->pos);

                if (loc.line < top)
                {
                    view->scroll_at += loc.line - top;
                }
                if (loc.line > bot)
                {
                    view->scroll_at += loc.line - bot;
                }

                PushRect(Layer_Text, view->viewport, COLOR_BLACK);
                DrawView(view, is_active_window);
            }
        }
    }
    else
    {
        DrawWindows(window->left);
        DrawWindows(window->right);
    }
}

function StringMap *
PushStringMap(Arena *arena, size_t size)
{
    StringMap *result = PushStruct(arena, StringMap);
    result->arena = arena;
    result->size = size;
    result->nodes = PushArray(arena, result->size, StringMapNode *);
    return result;
}

function void *
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

function void
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

// Ryan's text controls example: https://hatebin.com/ovcwtpsfmj

function void
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

function Bitmap
GetGlyphBitmap(Font *font, Glyph glyph)
{
    Rect2i rect = GetGlyphRect(font, glyph);
    Bitmap result = MakeBitmapView(&font->bitmap, rect);
    return result;
}

function void
ExecuteCommand(View *view, Command *command)
{
    switch (command->kind)
    {
        case Command_Basic:
        {
            Cursor *cursor = GetCursor(view);

            int64_t mark = cursor->mark;
            command->command(editor_state);

            if (mark == cursor->mark)
            {
                cursor->mark = cursor->pos;
            }
        } break;

        case Command_Text:
        {
            INVALID_CODE_PATH;
        } break;

        case Command_Movement:
        {
            Range range = command->movement(editor_state);
            if (editor_state->clutch)
            {
                SetCursor(view, range.end);
            }
            else
            {
                SetCursor(view, range.end, range.start);
            }
        } break;

        case Command_Change:
        {
            Cursor *cursor = GetCursor(view);

            int64_t mark = cursor->mark;

            Range edit_range = GetEditRange(cursor);
            edit_range.end += 1; // Why?

            command->change(editor_state, edit_range);

            if (mark == cursor->mark)
            {
                cursor->mark = cursor->pos;
            }
        } break;
    }
}

function void
HandleViewEvents(View *view)
{
    Buffer *buffer = GetBuffer(view);

    for (PlatformEvent *event = nullptr;
         platform->NextEvent(&event, PlatformEventFilter_ANY);
         )
    {
        BindingMap *bindings = &editor_state->bindings[editor_state->edit_mode];

        Cursor *cursor = GetCursor(view);
        BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
        if ((event->type == PlatformEvent_Text) && bindings->text_command)
        {
            String text = MakeString(event->text_length, event->text);
            bindings->text_command->text(editor_state, text);
        }
        else if (MatchFilter(event->type, PlatformEventFilter_Input) &&
                 event->pressed)
        {
            bool ctrl_down = event->ctrl_down;
            bool shift_down = event->shift_down;
            bool ctrl_shift_down = ctrl_down && shift_down;

#if 0
            if (event->input_code == PlatformInputCode_Shift)
            {
                editor_state->clutch = !editor_state->clutch;
                if (!editor_state->clutch)
                {
                    buffer->mark = buffer->cursor;
                }
            }
            else if (shift_down)
            {
                editor_state->clutch = true;
            }

            if (event->input_code == PlatformInputCode_Escape)
            {
                editor_state->clutch = false;
                buffer->mark = buffer->cursor;
            }
#else
            editor_state->clutch = shift_down;
#endif

            Binding *binding = &bindings->map[event->input_code];
            Command *command = binding->regular;
            if (ctrl_shift_down && binding->ctrl_shift)
            {
                command = binding->ctrl_shift;
            }
            else if (ctrl_down && binding->ctrl)
            {
                command = binding->ctrl;
            }
            else if (shift_down && binding->shift)
            {
                command = binding->shift;
            }

            if (command)
            {
                if (AreEqual(command->name, "RepeatLastCommand"_str))
                {
                    if (cursor->mark > cursor->mark)
                    {
                        Swap(cursor->pos, cursor->mark);
                    }
                    ExecuteCommand(view, editor_state->last_movement_for_change);
                    ExecuteCommand(view, editor_state->last_change);
                }
                else
                {
                    ExecuteCommand(view, command);
                    switch (command->kind)
                    {
                        case Command_Movement: { editor_state->last_movement = command; } break;
                        case Command_Change:
                        {
                            editor_state->last_movement_for_change = editor_state->last_movement;
                            editor_state->last_change = command;
                        } break;
                    }
                }
            }
        }
    }

    if (buffer->dirty && !buffer->tokenizing)
    {
        buffer->dirty = false;
        if (buffer->count < BUFFER_ASYNC_THRESHOLD)
        {
            TokenizeBuffer(buffer);
        }
        else
        {
            platform->AddJob(platform->low_priority_queue, buffer, TokenizeBufferJob);
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
        editor_state->font = LoadFontFromDisk(&editor_state->transient_arena, "font8x16.bmp"_str, 8, 16);
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

        editor_state->cpp_spec = PushCppLanguageSpec(&editor_state->transient_arena);

        editor_state->null_buffer = OpenNewBuffer("null"_str, Buffer_Indestructible|Buffer_ReadOnly);
        editor_state->message_buffer = OpenNewBuffer("messages"_str, Buffer_Indestructible|Buffer_ReadOnly);

        editor_state->null_view = OpenNewView(BufferID::Null());

        Buffer *scratch_buffer = OpenBufferFromFile("test_file.txt"_str);
        View *scratch_view = OpenNewView(scratch_buffer->id);

        editor_state->root_window.view = scratch_view->id;
        RecalculateViewBounds(&editor_state->root_window, render_state->viewport);
        editor_state->active_window = &editor_state->root_window;

        platform->PushTickEvent();
        platform->app_initialized = true;
    }

    editor_state->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor_state->text_mouse_p = editor_state->screen_mouse_p / GlyphDim(&editor_state->font);

    if (editor_state->active_window->split != WindowSplit_Leaf)
    {
        platform->ReportError(PlatformError_Nonfatal, "Hey, the active window was not a leaf window, that's busted.");
        while (!editor_state->active_window->split != WindowSplit_Leaf)
        {
            editor_state->active_window = editor_state->active_window->left;
        }
    }

    HandleViewEvents(GetView(editor_state->active_window->view));
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
