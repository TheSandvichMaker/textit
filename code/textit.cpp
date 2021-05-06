#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"
#include "textit_buffer.cpp"
#include "textit_view.cpp"

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
            }
        }
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
DrawFileBar(View *view, Rect2i bounds)
{
    Buffer *buffer = view->buffer;
    DrawLine(bounds.min + MakeV2i(2, 0), buffer->name, COLOR_BLACK, MakeColor(192, 255, 127));
}

static inline void
DrawTextArea(View *view, Rect2i bounds)
{
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);

    int64_t left = bounds.min.x + 2;
    V2i at_p = MakeV2i(left, bounds.max.y - 2);
    for (int64_t pos = 0; pos < ArrayCount(buffer->text);)
    {
        if (at_p.y <= bounds.min.y)
        {
            break;
        }

        if (pos == loc.pos)
        {
            PushTile(Layer_Text, at_p, MakeSprite('\0', COLOR_BLACK, COLOR_WHITE));
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
            if (at_p.x >= bounds.max.x)
            {
                at_p.x = left;
                at_p.y -= 1;
            }

            uint8_t b = buffer->text[pos];
            if (IsAsciiByte(b))
            {
                Sprite sprite = MakeSprite(buffer->text[pos]);
                if (pos == loc.pos)
                {
                    Swap(sprite.foreground, sprite.background);
                }
                PushTile(Layer_Text, at_p, sprite);
                at_p.x += 1;
            }
            else
            {
                Color foreground = MakeColor(255, 0, 0);
                Color background = MakeColor(0, 0, 0);
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
DrawView(View *view, Rect2i bounds)
{
    Rect2i top = MakeRect2iMinMax(MakeV2i(bounds.min.x, bounds.max.y - 1), bounds.max);
    Rect2i bot = MakeRect2iMinMax(bounds.min, MakeV2i(bounds.max.x, bounds.max.y - 1));

    PushRectOutline(Layer_Background, bounds, COLOR_WHITE, COLOR_BLACK);

    DrawFileBar(view, top);
    DrawTextArea(view, bot);
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

        platform->app_initialized = true;
    }

    editor_state->screen_mouse_p = MakeV2i(platform->mouse_x, platform->mouse_y);
    editor_state->text_mouse_p = editor_state->screen_mouse_p / GlyphDim(&editor_state->font);

    BeginRender();

    HandleViewInput(editor_state->open_view);
    DrawView(editor_state->open_view, render_state->viewport);

    EndRender();

    if (editor_state->debug_delay_frame_count > 0)
    {
        platform->SleepThread(editor_state->debug_delay);
        editor_state->debug_delay_frame_count -= 1;
    }
}
