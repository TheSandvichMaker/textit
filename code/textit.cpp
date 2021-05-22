#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_buffer.cpp"
#include "textit_view.cpp"
#include "textit_command.cpp"
#include "textit_base_commands.cpp"

static inline void
LoadDefaultBindings()
{
    BindingMap *command = &editor_state->bindings[EditMode_Command];
    command->text_command = nullptr;
    command->map[PlatformInputCode_Left].regular  = FindCommand("CMD_MoveLeft"_str);
    command->map[PlatformInputCode_Right].regular = FindCommand("CMD_MoveRight"_str);
    command->map[PlatformInputCode_Up].regular    = FindCommand("CMD_MoveUp"_str);
    command->map[PlatformInputCode_Down].regular  = FindCommand("CMD_MoveDown"_str);
    command->map[PlatformInputCode_Left].ctrl     = FindCommand("CMD_MoveLeftIdentifier"_str);
    command->map[PlatformInputCode_Right].ctrl    = FindCommand("CMD_MoveRightIdentifier"_str);
    command->map['H'].regular                     = FindCommand("CMD_MoveLeft"_str);
    command->map['J'].regular                     = FindCommand("CMD_MoveDown"_str);
    command->map['K'].regular                     = FindCommand("CMD_MoveUp"_str);
    command->map['L'].regular                     = FindCommand("CMD_MoveRight"_str);
    command->map['W'].regular                     = FindCommand("CMD_MoveRightIdentifier"_str);
    command->map['B'].regular                     = FindCommand("CMD_MoveLeftIdentifier"_str);
    command->map['I'].regular                     = FindCommand("CMD_EnterTextMode"_str);
    command->map['X'].regular                     = FindCommand("CMD_DeleteChar"_str);
    command->map['U'].regular                     = FindCommand("CMD_UndoOnce"_str);
    command->map['R'].regular                     = FindCommand("CMD_RedoOnce"_str);

    BindingMap *text = &editor_state->bindings[EditMode_Text];
    text->text_command = FindCommand("CMD_WriteText"_str);
    text->map[PlatformInputCode_Left].regular     = FindCommand("CMD_MoveLeft"_str);
    text->map[PlatformInputCode_Right].regular    = FindCommand("CMD_MoveRight"_str);
    text->map[PlatformInputCode_Up].regular       = FindCommand("CMD_MoveUp"_str);
    text->map[PlatformInputCode_Down].regular     = FindCommand("CMD_MoveDown"_str);
    text->map[PlatformInputCode_Left].ctrl        = FindCommand("CMD_MoveLeftIdentifier"_str);
    text->map[PlatformInputCode_Right].ctrl       = FindCommand("CMD_MoveRightIdentifier"_str);
    text->map[PlatformInputCode_Escape].regular   = FindCommand("CMD_EnterCommandMode"_str);
    text->map[PlatformInputCode_Back].regular     = FindCommand("CMD_BackspaceChar"_str);
    text->map[PlatformInputCode_Back].ctrl        = FindCommand("CMD_BackspaceWord"_str);
    text->map[PlatformInputCode_Delete].regular   = FindCommand("CMD_DeleteChar"_str);
    text->map[PlatformInputCode_Delete].ctrl      = FindCommand("CMD_DeleteWord"_str);
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

static inline void DrawView(View *view);

static inline void
HandleViewEvents(View *view)
{
    Buffer *buffer = view->buffer;

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

        BeginRender();
        view->viewport = render_state->viewport;
        DrawView(view);
        EndRender();
    }
}

static inline V2i
DrawLine(V2i p, String line, Color foreground, Color background)
{
    V2i at_p = p;
    for (size_t i = 0; i < line.size;)
    {
        if (IsUtf8Byte(line.data[i]))
        {
            ParseUtf8Result unicode = ParseUtf8Codepoint(&line.data[i]);
            String string = FormatTempString("\\u%x", unicode.codepoint);
            at_p = DrawLine(at_p, string, foreground, background);
            i += unicode.advance;
        }
        else if (IsPrintableAscii(line.data[i]))
        {
            Sprite sprite = MakeSprite(line.data[i], foreground, background);
            PushTile(Layer_Text, at_p, sprite);

            at_p.x += 1;
            i += 1;
        }
        else
        {
            String string = FormatTempString("\\x%02hhx", line.data[i]);
            at_p = DrawLine(at_p, string, foreground, background);
            i += 1;
        }
    }
    return at_p;
}

static inline void
DrawTextArea(View *view, Rect2i bounds)
{
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t left = bounds.min.x + 2;
    V2i at_p = MakeV2i(left, bounds.max.y - 2);

    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);
    Color unrenderable_text_foreground = GetThemeColor("unrenderable_text_foreground"_str);
    Color unrenderable_text_background = GetThemeColor("unrenderable_text_background"_str);

    int64_t scan_line = 0;
    int64_t pos = 0;
    while ((scan_line < view->scroll_at) &&
           (pos < buffer->count))
    {
        int64_t newline_length = PeekNewline(buffer, pos);
        if (newline_length)
        {
            pos += newline_length;
            scan_line += 1;
        }
        else
        {
            pos += 1;
        }
    }

    while (pos < buffer->count)
    {
        if (at_p.y <= bounds.min.y)
        {
            break;
        }

        if (pos == loc.pos)
        {
            PushTile(Layer_Text, at_p, MakeSprite('\0', text_background, text_foreground));
        }

        if (buffer->text[pos] == '\0')
        {
            break;
        }

        int64_t newline_length = PeekNewline(buffer, pos);
        if (newline_length)
        {
            at_p.x = left;
            at_p.y -= 1;

            pos += newline_length;
        }
        else
        {
            if (at_p.x >= (bounds.max.x - 2))
            {
                PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));
                at_p.x = left - 1;
                at_p.y -= 1;
            }

            uint8_t b = buffer->text[pos];
            if (IsAsciiByte(b))
            {
                Sprite sprite = MakeSprite(buffer->text[pos], text_foreground, text_background);
                if (pos == loc.pos)
                {
                    Swap(sprite.foreground, sprite.background);
                }
                PushTile(Layer_Text, at_p, sprite);
                at_p.x += 1;
                pos += 1;
            }
            else
            {
                Color foreground = unrenderable_text_foreground;
                Color background = unrenderable_text_background;
                if (pos == loc.pos)
                {
                    Swap(foreground, background);
                }
                ParseUtf8Result unicode = ParseUtf8Codepoint(&buffer->text[pos]);
                String string = FormatTempString("\\u%x", unicode.codepoint);
                for (size_t i = 0; i < string.size; ++i)
                {
                    PushTile(Layer_Text, at_p, MakeSprite(string.data[i], foreground, background));
                    at_p.x += 1;
                    if (at_p.x >= (bounds.max.x - 2))
                    {
                        PushTile(Layer_Text, at_p, MakeSprite('\\', MakeColor(127, 127, 127), text_background));
                        at_p.x = left - 1;
                        at_p.y -= 1;
                    }
                }
                pos += unicode.advance;
            }
        }
    }
}

static inline void
DrawView(View *view)
{
    Buffer *buffer = view->buffer;
    Rect2i bounds = view->viewport;

    Color text_foreground = GetThemeColor("text_foreground"_str);
    Color text_background = GetThemeColor("text_background"_str);
    Color filebar_text_foreground = GetThemeColor("filebar_text_foreground"_str);

    String filebar_text_background_str = "filebar_text_background"_str;
    switch (editor_state->edit_mode)
    {
        case EditMode_Text:
        {
            filebar_text_background_str = "filebar_text_background_text_mode"_str;
        } break;
    }
    Color filebar_text_background = GetThemeColor(filebar_text_background_str);

    int64_t width = GetWidth(bounds);

    Rect2i left, right;
    SplitRect2iHorizontal(bounds, width / 2, &left, &right);

    PushRectOutline(Layer_Background, left, text_foreground, text_background);
    DrawLine(MakeV2i(bounds.min.x + 2, bounds.max.y - 1),
             FormatTempString("%.*s - scroll: %d", StringExpand(buffer->name), view->scroll_at),
             filebar_text_foreground, filebar_text_background);

    DrawTextArea(view, left);
    PushRectOutline(Layer_Background, right, text_foreground, text_background);

    V2i at_p = MakeV2i(right.min.x + 2, right.max.y - 2);

    UndoNode *current = CurrentUndoNode(buffer);
    DrawLine(at_p, FormatTempString("At level: %llu", buffer->undo.depth), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Ordinal: %llu, Pos: %lld", current->ordinal, current->pos), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Forward: \"%.*s\"", StringExpand(current->forward)), text_foreground, text_background); at_p.y -= 1;
    DrawLine(at_p, FormatTempString("Backward: \"%.*s\"", StringExpand(current->backward)), text_foreground, text_background); at_p.y -= 1;
    at_p.y -= 1;

    uint32_t branch_index = 0;
    for (UndoNode *child = current->first_child;
         child;
         child = child->next_child)
    {
        DrawLine(at_p, FormatTempString("Branch %d%s", branch_index, (branch_index == current->selected_branch ? " (NEXT)" : "")), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Ordinal: %llu, Pos: %lld", child->ordinal, child->pos), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Forward: \"%.*s\"", StringExpand(child->forward)), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Backward: \"%.*s\"", StringExpand(child->backward)), text_foreground, text_background); at_p.y -= 1;
        branch_index += 1;
        at_p.y -= 1;
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

        editor_state->open_buffer = OpenFileIntoNewBuffer(StringLiteral("test_file.txt"));
        editor_state->open_view = NewView(editor_state->open_buffer);

        platform->app_initialized = true;
    }

    editor_state->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor_state->text_mouse_p = editor_state->screen_mouse_p / GlyphDim(&editor_state->font);

    HandleViewEvents(editor_state->open_view);
    editor_state->edit_mode = editor_state->next_edit_mode;

    if (editor_state->debug_delay_frame_count > 0)
    {
        platform->SleepThread(editor_state->debug_delay);
        editor_state->debug_delay_frame_count -= 1;
    }
}
