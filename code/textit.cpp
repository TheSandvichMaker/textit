#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_config.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_text_storage.cpp"
#include "textit_language.cpp"
#include "textit_buffer.cpp"
#include "textit_tokenizer.cpp"
#include "textit_view.cpp"
#include "textit_command.cpp"
#include "textit_theme.cpp"
#include "textit_auto_indent.cpp"
#include "textit_base_commands.cpp"
#include "textit_draw.cpp"
#include "textit_command_line.cpp"

// things missing:
// automatic line comment inserts
// autocomplete of any kind
// code indexing
// go to definition
// jump history
// multiple cursors
// local search (and replace)
// project wide search (and replace)
// saving lmao
// not making undo history take up one billion megabytes, have a way to flush it to disk
// undo tree visualization / ux (I mean that was supposed to be a thing)
// yank behaviour (yank inner vs outer / yank on delete / etc)
// registers
// refinement of the paradigm of course
// better text rendering
// better rendering
// hardware accelerated rendering
// rendering api that isn't dumb as bananacakes
// incremental tokenization
// incremental parsing for code index
// incremental updating of line cache
// piece table text storage?
// editing big files without keeping them entirely in memory??
// language support for multiple languages to test and refine things
// manhandling of indentation (again)
// macros

//
// PRIOR ART: https://github.com/helix-editor/helix <--- look at how this lad does things
// Markers:
//  - https://blog.atom.io/2015/06/16/optimizing-an-important-atom-primitive.html
//  - https://github.com/neovim/neovim/issues/4816
//
// nakst how2uniscribe: https://gist.github.com/nakst/ab7ae09ed943100ea13707fa4f42b548
//

function Cursor *
GetCursorInternal(ViewID view, BufferID buffer, bool make_if_missing)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value);
    uint64_t slot = hash % ArrayCount(editor->cursor_hash);

    CursorHashEntry *entry = editor->cursor_hash[slot];

    while (entry &&
           (entry->key.value != key.value))
    {
        entry = entry->next_in_hash;
    }

    if (!entry && make_if_missing)
    {
        if (!editor->first_free_cursor_hash_entry)
        {
            editor->first_free_cursor_hash_entry = PushStructNoClear(&editor->transient_arena, CursorHashEntry);
            editor->first_free_cursor_hash_entry->next_in_hash = nullptr;
        }
        entry = editor->first_free_cursor_hash_entry;
        editor->first_free_cursor_hash_entry = entry->next_in_hash;

        entry->next_in_hash = editor->cursor_hash[slot];
        editor->cursor_hash[slot] = entry;

        entry->key = key;

        ZeroStruct(&entry->cursor);
    }

    return entry ? &entry->cursor : nullptr;
}

function Cursor *
GetCursor(ViewID view, BufferID buffer)
{
    return GetCursorInternal(view, buffer, true);
}

function Cursor *
IterateCursors(ViewID view, BufferID buffer)
{
    return GetCursorInternal(view, buffer, false);
}

function Cursor *
GetCursor(View *view, Buffer *buffer)
{
    return GetCursor(view->id, (buffer ? buffer->id : view->buffer));
}

function void
DestroyCursor(ViewID view, BufferID buffer)
{
    CursorHashKey key;
    key.view   = view;
    key.buffer = buffer;
    uint64_t hash = HashIntegers(key.value);
    uint64_t slot = hash % ArrayCount(editor->cursor_hash);

    CursorHashEntry *entry = editor->cursor_hash[slot];
    if (entry)
    {
        editor->cursor_hash[slot] = entry->next_in_hash;

        entry->next_free = editor->first_free_cursor_hash_entry;
        editor->first_free_cursor_hash_entry = entry;
    }
}

function Buffer *
OpenNewBuffer(String buffer_name, BufferFlags flags = 0)
{
    BufferID id = {};
    if (editor->buffer_count < MAX_BUFFER_COUNT)
    {
        id = editor->free_buffer_ids[MAX_BUFFER_COUNT - editor->buffer_count - 1];
        id.generation += 1;

        editor->used_buffer_ids[editor->buffer_count] = id;
        editor->buffer_count += 1;
    }

    Buffer *result = BootstrapPushStruct(Buffer, arena);
    result->id                   = id;
    result->flags                = flags;
    result->name                 = PushString(&result->arena, buffer_name);
    result->undo.at              = &result->undo.root;
    result->undo.run_pos         = -1;
    result->undo.insert_pos      = -1;
    result->undo.current_ordinal = 1;
    result->indent_rules         = &editor->default_indent_rules;
    result->language             = &editor->null_language;

    result->tokens.SetCapacity(32'000'000);
    result->line_data.SetCapacity(8'000'000);

    AllocateTextStorage(result, TEXTIT_BUFFER_SIZE);

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

    String ext;
    SplitExtension(filename, &ext);

    for (LanguageSpec *spec = editor->first_language; spec; spec = spec->next)
    {
        for (int extension_index = 0; extension_index < spec->associated_extension_count; extension_index += 1)
        {
            if (AreEqual(spec->associated_extensions[extension_index], ext))
            {
                result->language = spec;
                break;
            }
        }
    }

    TokenizeBuffer(result);

    if (editor->buffer_count == 2)
    {
        // if there's exactly one buffer (plus the null buffer), I guess we'll use this to set the working directory
        String path = SplitPath(filename);
        platform->SetWorkingDirectory(path);
    }

    return result;
}

function Buffer *
GetBuffer(BufferID id)
{
    Buffer *result = editor->buffers[0];
    if (id.index > 0 && id.index < MAX_BUFFER_COUNT)
    {
        Buffer *buffer = editor->buffers[id.index];
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
    for (size_t i = 0; i < editor->buffer_count; i += 1)
    {
        BufferID test_id = editor->used_buffer_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor->free_buffer_ids[MAX_BUFFER_COUNT - editor->buffer_count] = id;
    editor->used_buffer_ids[used_id_index] = editor->used_buffer_ids[--editor->buffer_count];

    for (size_t i = 0; i < editor->view_count; i += 1)
    {
        ViewID view_id = editor->used_view_ids[i];
        DestroyCursor(view_id, id);
    }

    editor->buffers[id.index] = nullptr;

    buffer->line_data.Release();
    buffer->tokens.Release();
    Release(&buffer->arena);
}

function View *
OpenNewView(BufferID buffer)
{
    ViewID id = {};
    if (editor->view_count < MAX_VIEW_COUNT)
    {
        id = editor->free_view_ids[MAX_VIEW_COUNT - editor->view_count - 1];
        id.generation += 1;

        editor->used_view_ids[editor->view_count] = id;
        editor->view_count += 1;
    }

    View *result = BootstrapPushStruct(View, arena);
    result->id     = id;
    result->buffer = buffer;

    result->line_hashes      = PushArray(&result->arena, 2048, uint64_t);
    result->prev_line_hashes = PushArray(&result->arena, 2048, uint64_t);

    editor->views[result->id.index] = result;

    return result;
}

function View *
GetView(ViewID id)
{
    View *result = editor->views[0];
    if (id.index > 0 && id.index < MAX_VIEW_COUNT)
    {
        View *view = editor->views[id.index];
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
    for (size_t i = 0; i < editor->view_count; i += 1)
    {
        ViewID test_id = editor->used_view_ids[i];
        if (test_id == id)
        {
            used_id_index = i;
            break;
        }
    }
    editor->free_view_ids[MAX_VIEW_COUNT - editor->view_count] = id;
    editor->used_view_ids[used_id_index] = editor->used_view_ids[--editor->view_count];

    for (size_t i = 0; i < editor->buffer_count; i += 1)
    {
        BufferID buffer_id = editor->used_buffer_ids[i];
        DestroyCursor(id, buffer_id);
    }

    Release(&view->arena);
    editor->views[id.index] = nullptr;
}

function ViewID
DuplicateView(ViewID id)
{
    ViewID result = ViewID::Null();

    View *view = GetView(id);
    if (view != editor->null_view)
    {
        View *new_view = OpenNewView(view->buffer);

        Cursor *cursor = GetCursor(view);
        Cursor *new_cursor = GetCursor(new_view);

        new_cursor->sticky_col = cursor->sticky_col;
        new_cursor->selection  = cursor->selection;
        new_cursor->pos        = cursor->pos;

        new_view->scroll_at = view->scroll_at;
        result = new_view->id;
    }

    return result;
}

function Window *
AllocateWindow(void)
{
    if (!editor->first_free_window)
    {
        editor->first_free_window = PushStructNoClear(&editor->transient_arena, Window);
        editor->first_free_window->next = nullptr;

        editor->debug.allocated_window_count += 1;
    }
    Window *result = editor->first_free_window;
    editor->first_free_window = result->next;

    ZeroStruct(result);
    return result;
}

function void
SplitWindow(Window *window, WindowSplitKind split)
{
    Assert(window->split == WindowSplit_Leaf);

    ViewID view = window->view;

    if (window->parent &&
        window->parent->split == split)
    {
        Window *new_window = AllocateWindow();
        new_window->view = DuplicateView(view);
        new_window->parent = window->parent;
        new_window->prev = window;
        new_window->next = window->next;
        if (new_window->next)
        {
            new_window->next->prev = new_window;
        }
        else
        {
            new_window->parent->last_child = new_window;
        }
        window->next = new_window;

        editor->active_window = window;
    }
    else
    {
        window->split = split;

        Window *left = AllocateWindow();
        left->parent = window;
        left->view = view;

        Window *right = AllocateWindow();
        right->parent = window;
        right->view = DuplicateView(view);

        left->next = right;
        right->prev = left;
        window->first_child = left;
        window->last_child  = right;

        editor->active_window = left;
    }
}

function void
DestroyWindowInternal(Window *window)
{
    if (window->parent)
    {
        Window *parent = window->parent;
        if (window->prev) window->prev->next = window->next;
        if (window->next) window->next->prev = window->prev;
        if (window == parent->first_child) parent->first_child = window->next;
        if (window == parent->last_child)  parent->last_child  = window->prev;

        if (window->prev)
        {
            editor->active_window = window->prev;
        }
        else if (window->next)
        {
            editor->active_window = window->next;
        }
        else
        {
            editor->active_window = parent->first_child;
        }

        if (!parent->first_child)
        {
            if (window->split == WindowSplit_Leaf)
            {
                DestroyView(window->view);
            }

            DestroyWindowInternal(parent);
        }
        else if (parent->first_child == parent->last_child)
        {
            // NOTE: If the parent is a split with only one child,
            // we want to condense that so that the parent just
            // gets replaced by its child.
            Window *first_child = parent->first_child;

            first_child->parent = parent->parent;
            parent->split       = first_child->split;
            parent->view        = first_child->view;
            parent->first_child = parent->first_child->first_child;
            parent->last_child  = parent->last_child->last_child;

            first_child->next = editor->first_free_window;
            editor->first_free_window = first_child;

            for (Window *child = parent->first_child;
                 child;
                 child = child->next)
            {
                child->parent = parent;
            }

            editor->active_window = parent;
        }
        else if (window->split == WindowSplit_Leaf)
        {
            DestroyView(window->view);
        }

        window->next = editor->first_free_window;
        editor->first_free_window = window;
    }
}

function void
DestroyWindow(Window *window)
{
    DestroyWindowInternal(window);
    if (editor->active_window->split != WindowSplit_Leaf)
    {
        editor->active_window = editor->active_window->first_child;
    }
}

function void
RecalculateViewBounds(Window *window, Rect2i rect)
{
    if (window->split == WindowSplit_Leaf)
    {
        View *view = GetView(window->view);
        int64_t line_count = view->viewport.max.y - view->viewport.min.y;
        if (AreEqual(view->viewport.min, rect.min) &&
            AreEqual(view->viewport.max, rect.max))
        {
            Swap(view->prev_line_hashes, view->line_hashes);
            ZeroArray(line_count, view->line_hashes);
        }
        else
        {
            ZeroArray(line_count, view->prev_line_hashes);
            ZeroArray(line_count, view->line_hashes);
        }
        view->viewport = rect;
    }
    else
    {
        int child_count = 0;
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            child_count += 1;
        }

        int64_t dim   = ((window->split == WindowSplit_Vert) ? GetWidth(rect) : GetHeight(rect));
        int64_t start = ((window->split == WindowSplit_Vert) ? rect.min.x : rect.min.y);
        int64_t end   = ((window->split == WindowSplit_Vert) ? rect.max.x : rect.max.y);

        int child_index = 0;
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            int64_t split_start = start + child_index*dim / child_count;
            int64_t split_end   = start + (child_index + 1)*dim / child_count;
            if (!child->next)
            {
                split_end = end;
            }

            Rect2i child_rect = rect;
            if (window->split == WindowSplit_Vert)
            {
                child_rect.min.x = split_start;
                child_rect.max.x = split_end;
            }
            else
            {
                child_rect.min.y = split_start;
                child_rect.max.y = split_end;
            }
            RecalculateViewBounds(child, child_rect);

            child_index += 1;
        }
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

        bool is_active_window = (window == editor->active_window);

        ScopedClipRect clip_rect(Scale(view->viewport, editor->font_metrics), view->id);
        view->actual_viewport_line_height = DrawView(view, is_active_window);
    }
    else
    {
        for (Window *child = window->first_child;
             child;
             child = child->next)
        {
            DrawWindows(child);
        }
    }
}

function void
SetDebugDelay(int milliseconds, int frame_count)
{
    editor->debug.delay = milliseconds;
    editor->debug.delay_frame_count = frame_count;
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

function Font
LoadFontFromDisk(Arena *arena, String filename, int glyph_w, int glyph_h)
{
    Font result = {};

    String file = platform->ReadFile(platform->GetTempArena(), filename);
    if (!file.size)
    {
        platform->ReportError(PlatformError_Nonfatal,
                              "Could not open file '%.*s' while loading font.", StringExpand(filename));
        return result;
    }

    Bitmap bitmap = ParseBitmap(arena, file);
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
ApplyMove(Buffer *buffer, Cursor *cursor, Move move)
{
    int direction = (move.pos >= cursor->pos ? 1 : -1);
    if (editor->clutch)
    {
        cursor->selection.inner = Union(cursor->selection.inner, move.selection.inner);
        cursor->selection.outer = Union(cursor->selection.outer, move.selection.outer);
        cursor->pos = move.pos;
    }
    else
    {
        cursor->selection = move.selection;
        cursor->pos = move.pos;
    }
    while (IsInBufferRange(buffer, cursor->pos + direction) &&
           IsTrailingUtf8Byte(ReadBufferByte(buffer, cursor->pos)))
    {
        cursor->pos += direction;
    }
}

function void
ExecuteCommand(View *view, Command *command)
{
    switch (command->kind)
    {
        case Command_Basic:
        {
            command->command();
        } break;

        case Command_Text:
        {
            INVALID_CODE_PATH;
        } break;

        case Command_Movement:
        {
            Cursor *cursor = GetCursor(view);

            Move move = command->movement();
            ApplyMove(GetBuffer(view), cursor, move);

            editor->last_movement   = command; 
            editor->last_move_flags = move.flags;
        } break;

        case Command_Change:
        {
            Cursor *cursor = GetCursor(view);

            Selection selection;
            selection.inner = SanitizeRange(cursor->selection.inner);
            selection.outer = SanitizeRange(cursor->selection.outer);
            command->change(selection);

            if (editor->last_movement)
            {
                Assert(editor->last_movement->kind == Command_Movement);
                if (editor->last_move_flags & MoveFlag_NoAutoRepeat)
                {
                    cursor->selection = MakeSelection(cursor->pos);
                }
                else if (editor->next_edit_mode == EditMode_Command)
                {
                    Move next_move = editor->last_movement->movement();
                    ApplyMove(GetBuffer(view), cursor, next_move);
                }
            }
        } break;
    }
}

function bool
HandleViewEvent(ViewID view_id, const PlatformEvent &event)
{
    bool result = true;

    View *view = GetView(view_id);
    Buffer *buffer = GetBuffer(view);

    BindingMap *bindings = &editor->bindings[editor->edit_mode];

    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
    if ((event.type == PlatformEvent_Text) && bindings->text_command)
    {
        String text = event.text;
        bindings->text_command->text(text);
    }
    else if (MatchFilter(event.type, PlatformEventFilter_KeyDown))
    {
        bool ctrl_down  = event.ctrl_down;
        bool shift_down = event.shift_down;
        bool alt_down   = event.alt_down;

        Modifiers modifiers = 0;
        if (ctrl_down)  modifiers |= Modifier_Ctrl;
        if (shift_down) modifiers |= Modifier_Shift;
        if (alt_down)   modifiers |= Modifier_Alt;

        bool consumed_digit = false;
        if (!modifiers &&
            editor->edit_mode == EditMode_Command &&
            event.input_code >= PlatformInputCode_0 &&
            event.input_code <= PlatformInputCode_9)
        {
            if (event.input_code > PlatformInputCode_0 ||
                view->repeat_value > 0)
            {
                consumed_digit = true;

                int64_t digit = (int64_t)(event.input_code - PlatformInputCode_0);
                view->repeat_value *= 10;
                view->repeat_value += digit;

                if (view->repeat_value > 9999)
                {
                    view->repeat_value = 9999;
                }
            }
        }

        if (!consumed_digit)
        {
            editor->clutch = shift_down;

            Binding *binding = &bindings->by_code[event.input_code];
            Command *command = binding->by_modifiers[modifiers];
            if (!command)
            {
                modifiers &= ~Modifier_Shift;
                command = binding->by_modifiers[modifiers];
            }

            if (command)
            {
                if (modifiers & Modifier_Shift)
                {
                    // if this command was really bound with shift, don't enable the clutch
                    editor->clutch = false;
                }

                int64_t undo_ordinal = CurrentUndoOrdinal(buffer);

                int64_t repeat = Max(1, view->repeat_value);
                view->repeat_value = 0;

                if (AreEqual(command->name, "RepeatLastCommand"_str))
                {
                    for (int64_t i = 0; i < repeat*editor->last_repeat; i += 1)
                    {
                        ExecuteCommand(view, editor->last_movement_for_change);
                        ExecuteCommand(view, editor->last_change);
                    }
                    editor->last_movement = editor->last_movement_for_change;
                }
                else
                {
                    for (int64_t i = 0; i < repeat; i += 1)
                    {
                        ExecuteCommand(view, command);
                    }
                    switch (command->kind)
                    {
                        INCOMPLETE_SWITCH;
                        
                        case Command_Change:
                        {
                            editor->last_movement_for_change = editor->last_movement;
                            editor->last_change = command;
                        } break;
                    }

                    editor->last_repeat = repeat;
                }

                MergeUndoHistory(buffer, undo_ordinal, CurrentUndoOrdinal(buffer));

                editor->suppress_text_event = true;
            }
        }
    }
    else if (MatchFilter(event.type, PlatformEventFilter_Mouse))
    {
        if (event.input_code == PlatformInputCode_LButton)
        {
            if (event.pressed)
            {
                editor->mouse_down = true;
                OnMouseDown();
            }
            else
            {
                editor->mouse_down = false;
                OnMouseUp();
            }
        }
    }

    return result;
}

void
AppUpdateAndRender(Platform *platform_)
{
    platform = platform_;

    editor = (EditorState *)platform->persistent_app_data;
    if (!platform->app_initialized)
    {
        editor = BootstrapPushStruct(EditorState, permanent_arena);
        platform->persistent_app_data = editor;
    }

    if (platform->exe_reloaded)
    {
        RestoreGlobalState(&editor->global_state, &editor->transient_arena);
    }

    if (!platform->app_initialized)
    {
#if 0
        if (!platform->MakeAsciiFont("Px437 OlivettiThin 8x14"_str,
                                     &editor->font,
                                     14,
                                     PlatformFontRasterFlag_RasterFont))
        {
            platform->ReportError(PlatformError_Nonfatal, "Failed to load ttf font. Trying fallback bmp font.");
            editor->font = LoadFontFromDisk(&editor->transient_arena, "font8x16_slim.bmp"_str, 8, 16);
        }
#endif

        editor->font         = platform->CreateFont("Consolas"_str, 0, 15);
        editor->font_metrics = platform->GetFontMetrics(editor->font);

        platform->window_resize_snap_w = (int32_t)editor->font_metrics.x;
        platform->window_resize_snap_h = (int32_t)editor->font_metrics.y;

        InitializeRenderState(&editor->transient_arena, &platform->backbuffer.bitmap);

        LoadDefaultTheme();
        LoadDefaultBindings();
        LoadDefaultIndentRules(&editor->default_indent_rules);

        for (int16_t i = 0; i < MAX_BUFFER_COUNT; i += 1)
        {
            editor->free_buffer_ids[i].index = MAX_BUFFER_COUNT - i - 1;
        }

        for (int16_t i = 0; i < MAX_VIEW_COUNT; i += 1)
        {
            editor->free_view_ids[i].index = MAX_VIEW_COUNT - i - 1;
        }

        editor->null_language.name = "none"_str;
        PushCppLanguageSpec();

        editor->null_buffer = OpenNewBuffer("null"_str, Buffer_Indestructible|Buffer_ReadOnly);
        editor->null_view   = OpenNewView(BufferID::Null());

        Buffer *scratch_buffer = OpenBufferFromFile("code/textit.cpp"_str);
        View   *scratch_view   = OpenNewView(scratch_buffer->id);

        editor->root_window.view = scratch_view->id;
        editor->active_window = &editor->root_window;

        platform->PushTickEvent();
        platform->app_initialized = true;
    }

    editor->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor->text_mouse_p   = editor->screen_mouse_p / editor->font_metrics;

    if (editor->active_window->split != WindowSplit_Leaf)
    {
        platform->ReportError(PlatformError_Nonfatal, "Hey, the active window was not a leaf window, that's busted.");
        while (editor->active_window->split != WindowSplit_Leaf)
        {
            editor->active_window = editor->active_window->first_child;
        }
    }

    BeginRender();

    bool handled_any_events = false;

    if (editor->mouse_down)
    {
        OnMouseHeld();
    }

    PlatformEvent event;
    for (PlatformEventIterator it = platform->IterateEvents(PlatformEventFilter_ANY);
         platform->NextEvent(&it, &event);
         )
    {
        if (editor->suppress_text_event && event.type == PlatformEvent_Text)
        {
            editor->suppress_text_event = false;
            continue;
        }

        if (editor->command_line_count > 0)
        {
            editor->clutch = false;
            handled_any_events = HandleCommandLineEvent(editor->command_lines[editor->command_line_count - 1], event);
        }
        else
        {
            handled_any_events = HandleViewEvent(editor->active_window->view, event);
            if (editor->command_line_count > 0)
            {
                // if we began a command line, it needs to be warmed up to not show a frame of lag for the predictions
                CommandLine *cl = editor->command_lines[editor->command_line_count - 1];
                cl->GatherPredictions(cl);
            }
        }
    }

    {
        View *view = GetActiveView();
        Buffer *buffer = GetBuffer(view);

        if (view->next_buffer)
        {
            view->buffer = view->next_buffer;
            view->next_buffer = {};
        }

        if ((editor->next_edit_mode == EditMode_Text) &&
            (buffer->flags & Buffer_ReadOnly))
        {
            editor->next_edit_mode = EditMode_Command;
        }
        editor->edit_mode = editor->next_edit_mode;

        RecalculateViewBounds(&editor->root_window, render_state->viewport);
        DrawWindows(&editor->root_window);

        if (editor->command_line_count > 0)
        {
            DrawCommandLines();
        }
    }

    EndRender();

#if 0
    for (ViewIterator it = IterateViews(); IsValid(&it); Next(&it))
    {
        View *view = it.view;
        int64_t line_height = GetHeight(view->viewport);
        platform->SetTextClipRect(&platform->backbuffer, MakeRect2iMinDim(0, 0, platform->backbuffer.bitmap.w, platform->backbuffer.bitmap.h));
        for (int64_t line = 0; line < line_height; line += 1)
        {
            V2i p = editor->font_metrics*MakeV2i(view->viewport.max.x - 39, view->viewport.min.y + line);
            String text = PushTempStringF("0x%016llX (0x%016llX)", view->line_hashes[line], view->prev_line_hashes[line]);
            BlitRect(&platform->backbuffer.bitmap, MakeRect2iMinDim(p.x, p.y, text.size*editor->font_metrics.x, editor->font_metrics.y), COLOR_BLACK);
            platform->DrawText(editor->font, &platform->backbuffer, text, p, COLOR_WHITE, COLOR_BLACK);
        }
    }
#endif

    {
        Buffer *active_buffer = GetBuffer(GetView(editor->active_window->view));
        String leaf;
        SplitPath(active_buffer->name, &leaf);
        platform->window_title = PushTempStringF("%.*s - TextIt", StringExpand(leaf));
    }
                                             
    if (editor->debug.delay_frame_count > 0)
    {
        platform->SleepThread(editor->debug.delay);
        editor->debug.delay_frame_count -= 1;
    }
}
