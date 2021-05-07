#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"

// here's some japanese: この野郎！！

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

static inline Buffer *
NewBuffer(String buffer_name)
{
    Buffer *result = BootstrapPushStruct(Buffer, arena);
    result->name = PushString(&result->arena, buffer_name);
    result->undo_state->undo_sentinel.next = &result->undo_state->undo_sentinel;
    result->undo_state->undo_sentinel.prev = &result->undo_state->undo_sentinel;
    return result;
}

static inline Buffer *
OpenFileIntoNewBuffer(String filename)
{
    Buffer *result = NewBuffer(filename);
    size_t file_size = platform->ReadFileInto(sizeof(result->text), result->text, filename);
    result->count = (int32_t)file_size;
    return result;
}

static inline View *
NewView(Buffer *buffer)
{
    View *result = BootstrapPushStruct(View, arena);
    result->buffer = buffer;
    return result;
}

static inline Bitmap
GetGlyphBitmap(Font *font, Glyph glyph)
{
    Rect2i rect = GetGlyphRect(font, glyph);
    Bitmap result = MakeBitmapView(&font->bitmap, rect);
    return result;
}

static inline Range
MakeRange(int32_t start, int32_t end)
{
    Range result = {};
    result.start = start;
    result.end = end;
    if (result.start > result.end)
    {
        Swap(result.start, result.end);
    }
    return result;
}

static inline Range
MakeRange(int32_t pos)
{
    return MakeRange(pos, pos);
}

static inline Range
MakeRangeStartLength(int32_t start, int32_t length)
{
    return MakeRange(start, start + length);
}

static inline int32_t
ClampToRange(int32_t value, Range bounds)
{
    if (value < bounds.start) value = bounds.start;
    if (value > bounds.end  ) value = bounds.end;
    return value;
}

static inline Range
ClampRange(Range range, Range bounds)
{
    if (range.start < bounds.start) range.start = bounds.start;
    if (range.end   > bounds.end  ) range.end   = bounds.end;
    return range;
}

static inline Range
BufferRange(Buffer *buffer)
{
    return MakeRange(0, buffer->count);
}

static inline BufferLocation
ViewCursorToBufferLocation(Buffer *buffer, V2i cursor)
{
    if (cursor.y < 0) cursor.y = 0;

    int32_t at_pos     = 0;
    int32_t at_line    = 0;
    int32_t line_start = 0;
    int32_t line_end   = 0;
    while (at_pos < buffer->count && at_line < cursor.y)
    {
        if (buffer->text[at_pos] == '\n')
        {
            at_line += 1;
            line_start = at_pos + 1;
        }
        at_pos += 1;
    }
    line_end = line_start;
    while (line_end < buffer->count && buffer->text[line_end] != '\n')
    {
        line_end += 1;
    }

    BufferLocation result = {};
    result.line_number = at_line;
    result.line_range = MakeRange(line_start, line_end);
    result.pos = ClampToRange(line_start + cursor.x, BufferRange(buffer));
    if (result.pos > result.line_range.end)
    {
        result.pos = result.line_range.end;
    }
    return result;
}

static inline V2i
BufferPosToViewCursor(Buffer *buffer, int32_t pos)
{
    if (pos < 0) pos = 0; // TODO: Should maybe assert in debug
    if (pos > buffer->count) pos = buffer->count;

    int32_t at_pos  = 0;
    int32_t at_line = 0;
    int32_t at_col  = 0;
    while (at_pos < buffer->count && at_pos < pos)
    {
        if (buffer->text[at_pos] == '\n')
        {
            at_col = 0;
            at_line += 1;
        }
        else
        {
            at_col += 1;
        }
        at_pos += 1;
    }

    V2i result = MakeV2i(at_col, at_line);
    return result;
}

static inline void
SetCursorPos(View *view, int32_t pos)
{
    view->cursor = BufferPosToViewCursor(view->buffer, pos);
}

static inline void
MoveCursorRelative(View *view, V2i delta)
{
    view->cursor += delta;

    BufferLocation loc = ViewCursorToBufferLocation(view->buffer, view->cursor);
    int32_t line_length = loc.line_range.end - loc.line_range.start;

    if (view->cursor.x < 0) view->cursor.x = 0;
    if (view->cursor.y < 0) view->cursor.y = 0;
    if (view->cursor.y > loc.line_number) view->cursor.y = loc.line_number;

    if (delta.x != 0)
    {
        int32_t horz_delta = delta.x;
        if (horz_delta > 0) horz_delta = 0;

        if (view->cursor.x > line_length) view->cursor.x = line_length + horz_delta;
        if (view->cursor.x < 0) view->cursor.x = 0;
    }
}

static inline int32_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = ClampRange(range, BufferRange(buffer));

    int32_t delta = range.start - range.end + (int32_t)text.size;

    if (delta > 0)
    {
        for (int32_t pos = buffer->count + delta; pos > range.start; pos -= 1)
        {
            buffer->text[pos] = buffer->text[pos - 1];
        }
    }
    else if (delta < 0)
    {
        for (int32_t pos = range.start; pos < buffer->count; pos += 1)
        {
            buffer->text[pos] = buffer->text[pos + 1];
        }
    }

    buffer->count += delta;

    for (int32_t i = 0; i < text.size; i += 1)
    {
        buffer->text[range.start + i] = text.data[i];
    }

    int32_t edit_end = range.start;
    if (delta > 0)
    {
        edit_end += delta;
    }
    return edit_end;
}

static inline bool
IsPrintableAscii(uint8_t c)
{
    return (c >= ' ' && c <= '~');
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
                int32_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), text);
                SetCursorPos(view, pos);
            }
        }
        else
        {
            switch (event->input_code)
            {
                case PlatformInputCode_Back:
                {
                    MoveCursorRelative(view, MakeV2i(-1, 0));
                    int32_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos - 1, 1), {});
                    SetCursorPos(view, pos);
                } break;

                case PlatformInputCode_Delete:
                {
                    int32_t pos = BufferReplaceRange(buffer, MakeRangeStartLength(loc.pos, 1), {});
                    SetCursorPos(view, pos);
                } break;

                case PlatformInputCode_Return:
                {
                    int32_t pos = BufferReplaceRange(buffer, MakeRange(loc.pos), StringLiteral("\n"));
                    SetCursorPos(view, pos);
                } break;

                case PlatformInputCode_Left:
                {
                    MoveCursorRelative(view, MakeV2i(-1, 0));
                } break;

                case PlatformInputCode_Right:
                {
                    MoveCursorRelative(view, MakeV2i(1, 0));
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
DrawView(View *view, Rect2i bounds)
{
    Buffer *buffer = view->buffer;

    BufferLocation loc = ViewCursorToBufferLocation(buffer, view->cursor);
    platform->DebugPrint("cursor: %d %d\n", view->cursor.x, view->cursor.y);

    PushRectOutline(Layer_Background, bounds, COLOR_WHITE, COLOR_BLACK);
    int32_t left = bounds.min.x + 2;
    V2i at_p = MakeV2i(left, bounds.max.y - 2);
    for (size_t pos = 0; pos < ArrayCount(buffer->text); ++pos)
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

        if (IsInRect(bounds, at_p))
        {
            if (buffer->text[pos] == '\n')
            {
                at_p.x = left;
                at_p.y -= 1;
            }
            else
            {
                Sprite sprite = MakeSprite(buffer->text[pos]);
                if (pos == loc.pos)
                {
                    Swap(sprite.foreground, sprite.background);
                }
                PushTile(Layer_Text, at_p, sprite);
                at_p.x += 1;
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

        editor_state->open_buffer = OpenFileIntoNewBuffer(StringLiteral("..\\code\\textit.cpp"));
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
