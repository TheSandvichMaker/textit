#include "textit.hpp"

#include "textit_string.cpp"
#include "textit_global_state.cpp"
#include "textit_image.cpp"
#include "textit_render.cpp"

// Ryan's text controls example: https://hatebin.com/ovcwtpsfmj

/*
static const uint8_t utf8_mask[]           = { 0x80, 0xE0, 0xF0, 0xF8, };
static const uint8_t utf8_matching_value[] = { 0x00, 0xC0, 0xE0, 0xF0, };
*/

static inline bool
IsAsciiByte(uint8_t b)
{
    if ((b & 0x80) == 0x00) return true;
    return false;
}

static inline int
IsHeadUnicodeByte(uint8_t b)
{
    if ((b & 0xE0) == 0xC0) return true;
    if ((b & 0xF0) == 0xE0) return true;
    if ((b & 0xF8) == 0xF0) return true;
    return false;
}

static inline bool
IsTrailingUnicodeByte(uint8_t b)
{
    if ((b & 0xC0) == 0x80)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static inline bool
IsUnicodeByte(uint8_t b)
{
    return (IsHeadUnicodeByte(b) ||
            IsTrailingUnicodeByte(b));
}

struct ParseUtf8Result
{
    uint32_t codepoint;
    uint32_t advance;
};

static inline ParseUtf8Result
ParseUtf8Codepoint(uint8_t *text)
{
    // TODO: This is adapted old code, I can't guarantee its quality, but I think it is at least correct

    ParseUtf8Result result = {};

    uint32_t num_bytes = 0;

    uint8_t *at = text;
    uint32_t codepoint = 0;

    /*
    uint8_t utf8_mask[]           = { 0x80, 0xE0, 0xF0, 0xF8, };
    uint8_t utf8_matching_value[] = { 0x00, 0xC0, 0xE0, 0xF0, };
    */

    if (!at[0])
    {
        // @Note: We've been null terminated.
    }
    else
    {
        uint8_t utf8_mask[] =
        {
            (1 << 7) - 1,
            (1 << 5) - 1,
            (1 << 4) - 1,
            (1 << 3) - 1,
        };
        uint8_t utf8_matching_value[] = { 0, 0xC0, 0xE0, 0xF0 };
        if ((*at & ~utf8_mask[0]) == utf8_matching_value[0])
        {
            num_bytes = 1;
        }
        else if ((uint8_t)(*at & ~utf8_mask[1]) == utf8_matching_value[1])
        {
            num_bytes = 2;
        }
        else if ((uint8_t)(*at & ~utf8_mask[2]) == utf8_matching_value[2])
        {
            num_bytes = 3;
        }
        else if ((uint8_t)(*at & ~utf8_mask[3]) == utf8_matching_value[3])
        {
            num_bytes = 4;
        }
        else
        {
            /* This is some nonsense input */
            num_bytes = 0;
        }

        if (num_bytes)
        {
            uint32_t offset = 6*(num_bytes - 1);
            for (size_t byte_index = 0; byte_index < num_bytes; byte_index++)
            {
                if (byte_index == 0)
                {
                    codepoint = (*at & utf8_mask[num_bytes-1]) << offset;
                }
                else
                {
                    if (*at != 0)
                    {
                        codepoint |= (*at & ((1 << 6) - 1)) << offset;
                    }
                    else
                    {
                        /* This is some nonsense input */
                        codepoint = 0;
                        break;
                    }
                }
                offset -= 6;
                at++;
            }
        }
    }

    result.codepoint = codepoint;
    result.advance = num_bytes;

    return result;
}

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
    return result;
}

static inline LineEndKind
GuessLineEndKind(String string)
{
    int64_t lf   = 0;
    int64_t crlf = 0;
    for (size_t i = 0; i < string.size;)
    {
        if (string.data[i + 0] == '\r' &&
            string.data[i + 1] == '\n')
        {
            crlf += 1;
            i += 2;
        }
        else if (string.data[i] == '\n')
        {
            lf += 1;
            i += 1;
        }
        else
        {
            i += 1;
        }
    }

    LineEndKind result = LineEnd_LF;
    if (crlf > lf)
    {
        result = LineEnd_CRLF;
    }
    return result;
}

static inline Buffer *
OpenFileIntoNewBuffer(String filename)
{
    Buffer *result = NewBuffer(filename);
    size_t file_size = platform->ReadFileInto(sizeof(result->text), result->text, filename);
    result->count = (int64_t)file_size;
    result->line_end = GuessLineEndKind(MakeString(result->count, (uint8_t *)result->text));
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
MakeRange(int64_t start, int64_t end)
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
MakeRange(int64_t pos)
{
    return MakeRange(pos, pos);
}

static inline Range
MakeRangeStartLength(int64_t start, int64_t length)
{
    return MakeRange(start, start + length);
}

static inline int64_t
ClampToRange(int64_t value, Range bounds)
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

static inline int64_t
PeekNewline(uint8_t *text, int64_t offset)
{
    if (text[offset + 0] == '\r' &&
        text[offset + 1] == '\n')
    {
        return 2;
    }
    if (text[offset] == '\n')
    {
        return 1;
    }
    return 0;
}

static inline int64_t
PeekNewlineBackward(uint8_t *text, int64_t offset)
{
    if (text[offset - 0] == '\n' &&
        text[offset - 1] == '\r')
    {
        return 2;
    }
    if (text[offset] == '\n')
    {
        return 1;
    }
    return 0;
}

static inline BufferLocation
ViewCursorToBufferLocation(Buffer *buffer, V2i cursor)
{
    if (cursor.y < 0) cursor.y = 0;

    int64_t at_pos     = 0;
    int64_t at_line    = 0;
    int64_t line_start = 0;
    int64_t line_end   = 0;
    while ((at_pos < buffer->count) &&
           (at_line < cursor.y))
    {
        int64_t newline_length = PeekNewline(buffer->text, at_pos);
        if (newline_length)
        {
            at_pos  += newline_length;
            at_line += 1;
            line_start = at_pos;
        }
        else
        {
            at_pos += 1;
        }
    }
    line_end = line_start;
    while ((line_end < buffer->count) &&
           !PeekNewline(buffer->text, line_end))
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
BufferPosToViewCursor(Buffer *buffer, int64_t pos)
{
    if (pos < 0) pos = 0; // TODO: Should maybe assert in debug
    if (pos > buffer->count) pos = buffer->count;

    int64_t at_pos  = 0;
    int64_t at_line = 0;
    int64_t at_col  = 0;
    while (at_pos < buffer->count && at_pos < pos)
    {
        int64_t newline_length = PeekNewline(buffer->text, at_pos);
        if (newline_length)
        {
            at_pos += newline_length;
            at_col = 0;
            at_line += 1;
        }
        else
        {
            at_pos += 1;
            at_col += 1;
        }
    }

    V2i result = MakeV2i(at_col, at_line);
    return result;
}

static inline void
SetCursorPos(View *view, int64_t pos)
{
    view->cursor = BufferPosToViewCursor(view->buffer, pos);
}

static inline void
MoveCursorRelative(View *view, V2i delta)
{
    V2i old_cursor = view->cursor;
    V2i new_cursor = view->cursor + delta;

    BufferLocation loc = ViewCursorToBufferLocation(view->buffer, new_cursor);
    int64_t line_length = loc.line_range.end - loc.line_range.start;

    new_cursor.y = ClampToRange(new_cursor.y, MakeRange(0, loc.line_number));

    if (delta.x != 0)
    {
        int64_t old_x = ClampToRange(old_cursor.x, MakeRange(0, line_length));
        int64_t new_x = ClampToRange(old_x + delta.x, MakeRange(0, line_length));
        if (new_x != old_x)
        {
            view->cursor.x = new_x;
        }
    }
    view->cursor.y = new_cursor.y;
}

static inline int64_t
BufferReplaceRange(Buffer *buffer, Range range, String text)
{
    range = ClampRange(range, BufferRange(buffer));

    int64_t delta = range.start - range.end + (int64_t)text.size;

    if (delta > 0)
    {
        int64_t offset = delta;
        for (int64_t pos = buffer->count + delta; pos >= range.start + offset; pos -= 1)
        {
            buffer->text[pos] = buffer->text[pos - offset];
        }
    }
    else if (delta < 0)
    {
        int64_t offset = -delta;
        for (int64_t pos = range.start; pos < buffer->count - offset; pos += 1)
        {
            buffer->text[pos] = buffer->text[pos + offset];
        }
    }

    buffer->count += delta;

    for (size_t i = 0; i < text.size; i += 1)
    {
        buffer->text[range.start + i] = text.data[i];
    }

    int64_t edit_end = range.start;
    if (delta > 0)
    {
        edit_end += delta;
    }
    return edit_end;
}

static inline bool
IsPrintableAscii(uint8_t c)
{
    bool result = (c >= ' ' && c <= '~');
    return result;
}

static inline bool
IsAlphabeticAscii(uint8_t c)
{
    bool result = ((c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z'));
    return result;
}

static inline bool
IsNumericAscii(uint8_t c)
{
    bool result = ((c >= '0') && (c <= '9'));
    return result;
}

static inline bool
IsAlphanumericAscii(uint8_t c)
{
    bool result = (IsAlphabeticAscii(c) ||
                   IsNumericAscii(c));
    return result;
}

static inline bool
IsWhitespace(uint8_t c)
{
    bool result = ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
    return result;
}

static inline int64_t
ScanWordForward(Buffer *buffer, int64_t pos)
{
    if (IsAlphanumericAscii(Peek(buffer, pos)))
    {
        while (IsAlphanumericAscii(Peek(buffer, pos)))
        {
            pos += 1;
        }
    }
    else
    {
        while (!IsAlphanumericAscii(Peek(buffer, pos)) && !IsWhitespace(Peek(buffer, pos)))
        {
            pos += 1;
        }
    }
    while (IsWhitespace(Peek(buffer, pos)))
    {
        pos += 1;
    }
    return pos;
}

static inline int64_t
ScanWordBackward(Buffer *buffer, int64_t pos)
{
    while (IsWhitespace(Peek(buffer, pos - 1)))
    {
        pos -= 1;
    }
    if (IsAlphanumericAscii(Peek(buffer, pos - 1)))
    {
        while (IsAlphanumericAscii(Peek(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    else
    {
        while (!IsAlphanumericAscii(Peek(buffer, pos - 1)) && !IsWhitespace(Peek(buffer, pos - 1)))
        {
            pos -= 1;
        }
    }
    return pos;
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
                        int64_t newline_length = PeekNewlineBackward(buffer->text, loc.pos - 1);
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

        int64_t newline_length = PeekNewline(buffer->text, pos);
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
