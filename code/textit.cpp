#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_buffer.cpp"
#include "textit_view.cpp"

static inline StringMap *
PushStringMap(Arena *arena, size_t size)
{
    StringMap *result = PushStruct(arena, StringMap);
    result->arena = arena;
    result->size  = size;
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
    SetThemeColor("text_foreground"_str, MakeColor(192, 255, 255));
    SetThemeColor("text_background"_str, MakeColor(192, 0, 0));
    SetThemeColor("filebar_text_foreground"_str, MakeColor(0, 0, 0));
    SetThemeColor("filebar_text_background"_str, MakeColor(192, 255, 128));
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

TEXTIT_INLINE Color
LinearToSRGB(V3 linear)
{
    Color result = MakeColor((uint8_t)(SquareRoot(linear.x)*255.0f),
                             (uint8_t)(SquareRoot(linear.y)*255.0f),
                             (uint8_t)(SquareRoot(linear.z)*255.0f));
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
HandleViewInput(View *view)
{
    Buffer *buffer = view->buffer;

    for (PlatformEvent *event = nullptr;
         NextEvent(&event, PlatformEventFilter_Text|PlatformEventFilter_KeyDown);
         )
    {
        BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);
        if (event->type == PlatformEvent_Text)
        {
            String text = MakeString(event->text_length, event->text);
            if (IsPrintableAscii(text.data[0]))
            {
                int64_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), text);
                SetCursorPos(view, pos);
            }
        }
        else
        {
            bool ctrl_down = event->ctrl_down;
            switch (event->input_code)
            {
                case PlatformInputCode_Back:
                {
                    if (ctrl_down)
                    {
                        int64_t start_pos = loc.pos;
                        int64_t end_pos = ScanWordBackward(buffer, loc.pos);
                        int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
                        SetCursorPos(view, final_pos);
                    }
                    else
                    {
                        int64_t newline_length = PeekNewlineBackward(buffer, loc.pos - 1);
                        int64_t to_delete = 1;
                        if (newline_length)
                        {
                            to_delete = newline_length;
                        }
                        int64_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos - to_delete, to_delete), {});
                        SetCursorPos(view, pos);
                    }
                } break;

                case PlatformInputCode_Delete:
                {
                    if (ctrl_down)
                    {
                        int64_t start_pos = loc.pos;
                        int64_t end_pos = ScanWordForward(buffer, loc.pos);
                        int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
                        SetCursorPos(view, final_pos);
                    }
                    else
                    {
                        int64_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos, 1), {});
                        SetCursorPos(view, pos);
                    }
                } break;

                case PlatformInputCode_Return:
                {
                    int64_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), LineEndString(buffer->line_end));
                    SetCursorPos(view, pos);
                } break;

                case PlatformInputCode_Left:
                {
                    if (ctrl_down)
                    {
                        int64_t pos = ScanWordBackward(buffer, loc.pos);
                        pos = ClampToRange(pos, loc.line_range);
                        SetCursorPos(view, pos);
                    }
                    else
                    {
                        MoveCursorRelative(view, MakeV2i(-1, 0));
                    }
                } break;

                case PlatformInputCode_Right:
                {
                    if (ctrl_down)
                    {
                        int64_t pos = ScanWordForward(buffer, loc.pos);
                        pos = ClampToRange(pos, loc.line_range);
                        SetCursorPos(view, pos);
                    }
                    else
                    {
                        MoveCursorRelative(view, MakeV2i(1, 0));
                    }
                } break;

                case PlatformInputCode_Up:
                {
                    MoveCursorRelative(view, MakeV2i(0, -1));
                } break;

                case PlatformInputCode_Down:
                {
                    MoveCursorRelative(view, MakeV2i(0, 1));
                } break;

                case 'Z':
                {
                    if (ctrl_down)
                    {
                        UndoOnce(buffer);
                    }
                } break;
            }
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
}

static inline void
DrawLine(V2i p, String line, Color foreground, Color background)
{
    V2i at_p = p;
    for (size_t i = 0; i < line.size; ++i)
    {
        Sprite sprite = MakeSprite(line.data[i], foreground, background);
        PushTile(Layer_Text, at_p, sprite);
        at_p.x += 1;
    }
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
            }
            else
            {
                Color foreground = unrenderable_text_foreground;
                Color background = unrenderable_text_background;
                if (pos == loc.pos)
                {
                    Swap(foreground, background);
                }
                String hex = FormatTempString("x%hhX", b);
                DrawLine(at_p, hex, foreground, background);
                at_p.x += hex.size;
            }
            pos += 1;
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
    Color filebar_text_background = GetThemeColor("filebar_text_background"_str);

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
    for (UndoNode *node = buffer->undo_state.undo_sentinel.next;
         node !=  &buffer->undo_state.undo_sentinel;
         node = node->next)
    {
        DrawLine(at_p, FormatTempString("Forward: %.*s", StringExpand(node->forward)), text_foreground, text_background); at_p.y -= 1;
        DrawLine(at_p, FormatTempString("Backward: %.*s", StringExpand(node->backward)), text_foreground, text_background); at_p.y -= 1;
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

        editor_state->open_buffer = OpenFileIntoNewBuffer(StringLiteral("test_file.cpp"));
        editor_state->open_view = NewView(editor_state->open_buffer);

        LoadDefaultTheme();

        platform->app_initialized = true;
    }

    editor_state->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor_state->text_mouse_p = editor_state->screen_mouse_p / GlyphDim(&editor_state->font);

    BeginRender();

    View *view = editor_state->open_view;
    view->viewport = render_state->viewport;

    HandleViewInput(editor_state->open_view);
    DrawView(editor_state->open_view);

    EndRender();

    if (editor_state->debug_delay_frame_count > 0)
    {
        platform->SleepThread(editor_state->debug_delay);
        editor_state->debug_delay_frame_count -= 1;
    }
}
