#include "textit.hpp"

#include "textit_sort.cpp"
#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_config.cpp"
#include "textit_image.cpp"
#include "textit_glyph_cache.cpp"
#include "textit_render.cpp"
#include "textit_text_storage.cpp"
#include "textit_tokens.cpp"
#include "textit_language.cpp"
#include "textit_line_index.cpp"
#include "textit_buffer.cpp"
#include "textit_tokenizer.cpp"
#include "textit_tags.cpp"
#include "textit_project.cpp"
#include "textit_view.cpp"
#include "textit_window.cpp"
#include "textit_cursor.cpp"
#include "textit_command.cpp"
#include "textit_theme.cpp"
#include "textit_auto_indent.cpp"
#include "textit_base_commands.cpp"
#include "textit_draw.cpp"
#include "textit_command_line.cpp"

#include "textit_language_cpp.cpp"
#include "textit_language_lua.cpp"

// things missing:
// local search and replace
// autocomplete of any kind
// multiple cursors -- needs to be thought about implementation robustness
// project wide search (and replace)
// not making undo history take up one billion megabytes, have a way to flush it to disk
// undo tree visualization / ux (I mean that was supposed to be a thing)
// yank behaviour (yank inner vs outer / yank on delete / etc)
// registers
// sensible scrolling in rendering
// hardware accelerated rendering
// rendering api that isn't dumb as bananacakes
// incremental parsing for code index
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

    font.w       = bitmap.w;
    font.h       = bitmap.h;
    font.pitch   = bitmap.pitch;
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
    BufferID prev_buffer = view->buffer;

    Cursor *base_cursor = GetCursor(view);
    int64_t prev_pos = base_cursor->pos;

    if (command->kind == Command_Basic)
    {
        command->command();
    }
    else
    {
        for (Cursor *cursor = IterateCursors(view->id, view->buffer);
             cursor;
             cursor = cursor->next)
        {
            editor->override_cursor = cursor;
            switch (command->kind)
            {
                case Command_Text:
                {
                    INVALID_CODE_PATH;
                } break;

                case Command_Movement:
                {
                    Move move = command->movement();
                    ApplyMove(GetBuffer(view), cursor, move);

                    editor->last_movement = command; 
                } break;

                case Command_Change:
                {
                    Selection selection;
                    selection.inner = SanitizeRange(cursor->selection.inner);
                    selection.outer = SanitizeRange(cursor->selection.outer);
                    command->change(selection);

                    if (editor->last_movement)
                    {
                        Assert(editor->last_movement->kind == Command_Movement);

                        if (editor->last_movement->flags & Movement_NoAutoRepeat)
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
    }
    editor->override_cursor = nullptr;

    if (command->flags & Command_Jump)
    {
        Buffer *buffer = GetBuffer(view);
        int64_t prev_line = GetLineNumber(buffer, prev_pos);
        int64_t next_line = GetLineNumber(buffer, base_cursor->pos);
        if ((view->next_buffer && view->next_buffer != view->buffer) || Abs(prev_line - next_line) > 1)
        {
            SaveJump(view, view->buffer, prev_pos);
        }
    }
}

function bool
HandleViewEvent(ViewID view_id, PlatformEvent *event)
{
    bool result = true;

    View   *view   = GetView(view_id);
    Buffer *buffer = GetBuffer(view);

    BindingMap *bindings = &editor->bindings[editor->edit_mode];

    if ((event->type == PlatformEvent_Text) && bindings->text_command)
    {
        String text = GetText(event);
        for (Cursor *cursor = IterateCursors(view->id, buffer->id);
             cursor;
             cursor = cursor->next)
        {
            editor->override_cursor = cursor;
            bindings->text_command->text(text);
        }
        editor->override_cursor = nullptr;
    }
    else if (MatchFilter(event->type, PlatformEventFilter_KeyDown))
    {
        bool ctrl_down  = event->ctrl_down;
        bool shift_down = event->shift_down;
        bool alt_down   = event->alt_down;

        Modifiers modifiers = 0;
        if (ctrl_down)  modifiers |= Modifier_Ctrl;
        if (shift_down) modifiers |= Modifier_Shift;
        if (alt_down)   modifiers |= Modifier_Alt;

        bool consumed_digit = false;
        if (!modifiers &&
            editor->edit_mode == EditMode_Command &&
            event->input_code >= PlatformInputCode_0 &&
            event->input_code <= PlatformInputCode_9)
        {
            if (event->input_code > PlatformInputCode_0 ||
                view->repeat_value > 0)
            {
                consumed_digit = true;

                int64_t digit = (int64_t)(event->input_code - PlatformInputCode_0);
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

            Binding *binding = &bindings->by_code[event->input_code];
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
    else if (MatchFilter(event->type, PlatformEventFilter_Mouse))
    {
        if (event->input_code == PlatformInputCode_LButton)
        {
            if (event->pressed)
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

function void
DestroyFont(PlatformFontHandle *handle)
{
    if (*handle) { platform->DestroyFont(*handle); *handle = NULL; }
}

function void
SetEditorFont(String name, int size, PlatformFontQuality quality)
{
    V2i max_metrics = MakeV2i(0, 0);
    V2i min_metrics = MakeV2i(INT_MAX, INT_MAX);
    editor->font_metrics = {};
    for (int i = 0; i < TextStyle_Count; i += 1)
    {
        DestroyFont(&editor->fonts[i]);
        editor->fonts[i] = platform->CreateFont(name, i, quality, size);

        V2i metrics = platform->GetFontMetrics(editor->fonts[i]);
        if (min_metrics.x > metrics.x) min_metrics.x = metrics.x;
        if (min_metrics.y > metrics.y) min_metrics.y = metrics.y;
        if (max_metrics.x < metrics.x) max_metrics.x = metrics.x;
        if (max_metrics.y < metrics.y) max_metrics.y = metrics.y;
    }

    if (!editor->font_name.capacity)
    {
        editor->font_name = MakeStringContainer(ArrayCount(editor->font_name_storage), editor->font_name_storage);
    }

    editor->font_metrics        = min_metrics;
    editor->font_max_glyph_size = MakeV2i(max_metrics.x + 2, max_metrics.y); // why are these metrics insufficient??

    Replace(&editor->font_name, name);
    editor->font_size    = size;
    editor->font_quality = quality;
    RebuildGlyphCache();

    platform->window_resize_snap_w = (int32_t)editor->font_metrics.x;
    platform->window_resize_snap_h = (int32_t)editor->font_metrics.y;
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
        SetEditorFont(core_config->font_name, 15, PlatformFontQuality_SubpixelAA);

        InitializeRenderState(&editor->transient_arena, &platform->backbuffer.bitmap);

        LoadDefaultThemes();
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

        editor->null_buffer = OpenNewBuffer("null"_str, Buffer_Indestructible|Buffer_ReadOnly);
        editor->null_view   = OpenNewView(BufferID::Null());

        DllInit(&editor->project_sentinel);

        Buffer *scratch_buffer = OpenBufferFromFile("code/textit.cpp"_str);
        View   *scratch_view   = OpenNewView(scratch_buffer->id);

        editor->root_window.view = scratch_view->id;
        editor->active_window = &editor->root_window;

        editor->search = MakeStringContainer(ArrayCount(editor->search_storage), editor->search_storage);

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

    int events_handled = 0;

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

        events_handled += 1;

        if (editor->command_line_count > 0)
        {
            editor->clutch = false;
            handled_any_events |= HandleCommandLineEvent(editor->command_lines[editor->command_line_count - 1], &event);
        }
        else
        {
            handled_any_events |= HandleViewEvent(editor->active_window->view, &event);
            if (editor->command_line_count > 0)
            {
                // if we began a command line, it needs to be warmed up to not show a frame of lag for the predictions
                CommandLine *cl = editor->command_lines[editor->command_line_count - 1];
                if (cl->GatherPredictions) cl->GatherPredictions(cl);
            }
        }
    }

    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (buffer->dirty)
        {
            buffer->dirty = false;
            ParseTags(buffer);
        }
    }

    for (ViewIterator it = IterateViews(); IsValid(&it); Next(&it))
    {
        View *view = it.view;
        if (view->next_buffer)
        {
            view->buffer = view->next_buffer;
            view->next_buffer = {};

            Buffer *buffer = GetBuffer(view);
            buffer->flags &= ~Buffer_Hidden;
        }
    }

    {
        View   *view   = GetActiveView();
        Buffer *buffer = GetBuffer(view);

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

    if (core_config->debug_show_glyph_cache)
    {
        int backbuffer_w = platform->backbuffer.bitmap.w;
        int glyph_tex_w = render_state->glyph_texture.bitmap.w;
        PushBitmap(&render_state->glyph_texture.bitmap, MakeV2i(backbuffer_w - glyph_tex_w, 0));
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

    if (handled_any_events) 
    {
#if 0
        platform->DebugPrint("Line index insert time: %fms, lookup time: %fms, lookup count: %lld, recursions per lookup: %lld\n",
                             1000.0*editor->debug.line_index_insert_timing,
                             1000.0*editor->debug.line_index_lookup_timing,
                             editor->debug.line_index_lookup_count,
                             editor->debug.line_index_lookup_recursion_count / editor->debug.line_index_lookup_count);
        platform->DebugPrint("Buffer move time: %fms\n", 1000.0*editor->debug.buffer_edit_timing);
#endif
    }
    editor->debug.line_index_insert_timing          = 0.0;
    editor->debug.line_index_lookup_timing          = 0.0;
    editor->debug.line_index_lookup_count           = 0;
    editor->debug.line_index_lookup_recursion_count = 0;
    editor->debug.buffer_edit_timing                = 0.0;

    for (ProjectIterator it = IterateProjects(); IsValid(&it); Next(&it))
    {
        if (it.project->associated_buffer_count <= 0)
        {
            DestroyCurrent(&it);
        }
    }

    {
        Buffer *active_buffer = GetActiveBuffer();
        Project *project = active_buffer->project;

        if (editor->active_project != project)
        {
            editor->active_project = project;
            platform->SetWorkingDirectory(SplitPath(active_buffer->full_path));
        }

        String leaf;
        SplitPath(active_buffer->name, &leaf);

        platform->window_title = PushTempStringF("%.*s - TextIt (project root: %.*s)",
                                                 StringExpand(leaf),
                                                 StringExpand(project->root));
    }
                                             
    if (editor->debug.delay_frame_count > 0)
    {
        platform->SleepThread(editor->debug.delay);
        editor->debug.delay_frame_count -= 1;
    }
}
