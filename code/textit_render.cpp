function void
RebuildGlyphCache()
{
    uint32_t glyph_cache_size = 2048; 
    uint32_t glyphs_per_row   = 32; // just so I can visualize easier I am putting this as a set width
    uint32_t glyphs_per_col   = (glyph_cache_size + glyphs_per_row - 1) / glyphs_per_row;

    platform->DestroyOffscreenBuffer(&render_state->glyph_texture);
    platform->CreateOffscreenBuffer((int)(glyphs_per_row*editor->font_max_glyph_size.x),
                                    (int)(glyphs_per_col*editor->font_max_glyph_size.y),
                                    &render_state->glyph_texture);
    ResizeGlyphCache(&render_state->glyph_cache, glyph_cache_size);

    render_state->glyphs_per_row = glyphs_per_row;
    render_state->glyphs_per_col = glyphs_per_col;
}

function void
InitializeRenderState(Arena *arena, Bitmap *target)
{
    render_state->target = target;

    render_state->cb_size = Megabytes(4); // random choice
    render_state->command_buffer = PushArrayNoClear(arena, render_state->cb_size, char);

    int64_t viewport_w = target->w / editor->font_metrics.x;
    int64_t viewport_h = target->h / editor->font_metrics.y;
    render_state->viewport = MakeRect2iMinDim(0, 0, viewport_w, viewport_h);

    render_state->wall_segment_lookup[Wall_Top|Wall_Bottom]                                          = 179;
    render_state->wall_segment_lookup[Wall_Top|Wall_Bottom|Wall_Left]                                = 180;
    render_state->wall_segment_lookup[Wall_Top|Wall_Bottom|Wall_LeftThick]                           = 181;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_BottomThick|Wall_Left]                      = 182;
    render_state->wall_segment_lookup[Wall_Left|Wall_BottomThick]                                    = 183;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_Bottom]                                    = 184;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_TopThick|Wall_BottomThick]                 = 185;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_BottomThick]                                = 186;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_BottomThick]                               = 187;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_TopThick]                                  = 188;
    render_state->wall_segment_lookup[Wall_Left|Wall_TopThick]                                       = 189;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_Top]                                       = 190;
    render_state->wall_segment_lookup[Wall_Left|Wall_Bottom]                                         = 191;
    render_state->wall_segment_lookup[Wall_Right|Wall_Top]                                           = 192;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right|Wall_Top]                                 = 193;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right|Wall_Bottom]                              = 194;
    render_state->wall_segment_lookup[Wall_Top|Wall_Bottom|Wall_Right]                               = 195;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right]                                          = 196;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right|Wall_Top|Wall_Bottom]                     = 197;
    render_state->wall_segment_lookup[Wall_Top|Wall_Bottom|Wall_RightThick]                          = 198;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_BottomThick|Wall_Right]                     = 199;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_RightThick]                                 = 200;
    render_state->wall_segment_lookup[Wall_BottomThick|Wall_RightThick]                              = 201;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_RightThick|Wall_TopThick]                  = 202;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_RightThick|Wall_BottomThick]               = 203;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_BottomThick|Wall_RightThick]                = 204;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_RightThick]                                = 205;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_BottomThick|Wall_LeftThick|Wall_RightThick] = 206;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_RightThick|Wall_Top]                       = 207;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right|Wall_TopThick]                            = 208;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_RightThick|Wall_Bottom]                    = 209;
    render_state->wall_segment_lookup[Wall_Left|Wall_Right|Wall_BottomThick]                         = 210;
    render_state->wall_segment_lookup[Wall_TopThick|Wall_Right]                                      = 211;
    render_state->wall_segment_lookup[Wall_Top|Wall_RightThick]                                      = 212;
    render_state->wall_segment_lookup[Wall_RightThick|Wall_Bottom]                                   = 213;
    render_state->wall_segment_lookup[Wall_BottomThick|Wall_Right]                                   = 214;
    render_state->wall_segment_lookup[Wall_Left|Wall_TopThick|Wall_BottomThick|Wall_Right]           = 215;
    render_state->wall_segment_lookup[Wall_LeftThick|Wall_Top|Wall_Bottom|Wall_RightThick]           = 216;
    render_state->wall_segment_lookup[Wall_Left|Wall_Top]                                            = 217;
    render_state->wall_segment_lookup[Wall_Right|Wall_Bottom]                                        = 218;
}

function V2i
ViewportRelative(V2i p)
{
    p.y = render_state->viewport.max.y - p.y - 1;
    return p;
}

function Bitmap
PushBitmap(Arena *arena, int w, int h)
{
    Bitmap result = {};
    result.w = w;
    result.h = h;
    result.pitch = w;
    result.data = PushArray(arena, w*h, Color);
    return result;
}

function Sprite
MakeWall(uint32_t wall_flags, Color foreground = COLOR_WHITE, Color background = COLOR_BLACK)
{
    Sprite result = {};
    result.foreground = foreground;
    result.background = background;

    if (wall_flags < ArrayCount(render_state->wall_segment_lookup))
    {
        result.glyph = render_state->wall_segment_lookup[wall_flags];
    }

    if (!result.glyph)
    {
        result.glyph = '?';
    }

    return result;
}

function void
ClearBitmap(Bitmap *bitmap)
{
    Color *row = bitmap->data;
    for (int64_t y = 0; y < bitmap->h; ++y)
    {
        ZeroSize(sizeof(Color)*bitmap->w, row);
        row += bitmap->pitch;
    }
}

function void
BlitRect(Bitmap *bitmap, Rect2i rect, Color color)
{
    rect = Intersect(rect, MakeRect2iMinMax(MakeV2i(0, 0), MakeV2i(bitmap->w, bitmap->h)));
    for (int64_t y = rect.min.y; y < rect.max.y; ++y)
    for (int64_t x = rect.min.x; x < rect.max.x; ++x)
    {
        bitmap->data[y*bitmap->pitch + x] = color;
    }
}

function void
BlitBitmap(Bitmap *dest, Bitmap *source, V2i p)
{
    int64_t source_min_x = Max(0, -p.x);
    int64_t source_min_y = Max(0, -p.y);
    int64_t source_max_x = Min(source->w, dest->w - p.x);
    int64_t source_max_y = Min(source->h, dest->h - p.y);
    int64_t source_adjusted_w = source_max_x - source_min_x;
    int64_t source_adjusted_h = source_max_y - source_min_y;

    p = Clamp(p, MakeV2i(0, 0), MakeV2i(dest->w, dest->h));

    Color *source_row = source->data + source_min_y*source->pitch + source_min_x;
    Color *dest_row = dest->data + p.y*dest->pitch + p.x;
    for (int64_t y = 0; y < source_adjusted_h; ++y)
    {
        Color *source_pixel = source_row;
        Color *dest_pixel = dest_row;
        for (int64_t x = 0; x < source_adjusted_w; ++x)
        {
            *dest_pixel++ = *source_pixel++;
        }
        source_row += source->pitch;
        dest_row += dest->pitch;
    }
}

function void
BlitBitmapAlphaMasked(Bitmap *dest, Bitmap *source, V2i p)
{
    int64_t source_min_x = Max(0, -p.x);
    int64_t source_min_y = Max(0, -p.y);
    int64_t source_max_x = Min(source->w, dest->w - p.x);
    int64_t source_max_y = Min(source->h, dest->h - p.y);
    int64_t source_adjusted_w = source_max_x - source_min_x;
    int64_t source_adjusted_h = source_max_y - source_min_y;

    p = Clamp(p, MakeV2i(0, 0), MakeV2i(dest->w, dest->h));

    Color *source_row = source->data + source_min_y*source->pitch + source_min_x;
    Color *dest_row   = dest->data + p.y*dest->pitch + p.x;
    for (int64_t y = 0; y < source_adjusted_h; ++y)
    {
        Color *source_pixel = source_row;
        Color *dest_pixel   = dest_row;
        for (int64_t x = 0; x < source_adjusted_w; ++x)
        {
            Color source_color = *source_pixel++;
            if (source_color.a)
            {
                *dest_pixel++ = source_color;
            }
            else
            {
                dest_pixel++;
            }
        }
        source_row += source->pitch;
        dest_row   += dest->pitch;
    }
}

function void
BlitBitmapSubpixelBlend(Bitmap *dest, Bitmap *source, V2i p, Color blend_color)
{
    int64_t source_min_x = Max(0, -p.x);
    int64_t source_min_y = Max(0, -p.y);
    int64_t source_max_x = Min(source->w, dest->w - p.x);
    int64_t source_max_y = Min(source->h, dest->h - p.y);
    int64_t source_adjusted_w = source_max_x - source_min_x;
    int64_t source_adjusted_h = source_max_y - source_min_y;

    p = Clamp(p, MakeV2i(0, 0), MakeV2i(dest->w, dest->h));

    float blend_r = (1.0f / 255.0f)*blend_color.r;
    float blend_g = (1.0f / 255.0f)*blend_color.g;
    float blend_b = (1.0f / 255.0f)*blend_color.b;

    Color *source_row = source->data + source_min_y*source->pitch + source_min_x;
    Color *dest_row   = dest->data + p.y*dest->pitch + p.x;
    for (int64_t y = 0; y < source_adjusted_h; ++y)
    {
        Color *source_pixel = source_row;
        Color *dest_pixel   = dest_row;
        for (int64_t x = 0; x < source_adjusted_w; ++x)
        {
            Color source_color = *source_pixel++;
            Color dest_color   = *dest_pixel;

            float source_r = 1.0f - (1.0f / 255.0f)*source_color.r;
            float source_g = 1.0f - (1.0f / 255.0f)*source_color.g;
            float source_b = 1.0f - (1.0f / 255.0f)*source_color.b;

            float dest_r   = (1.0f / 255.0f)*dest_color.r;
            float dest_g   = (1.0f / 255.0f)*dest_color.g;
            float dest_b   = (1.0f / 255.0f)*dest_color.b;

            Color blend;
            blend.r = (uint8_t)(255.0f*((1.0f - source_r)*dest_r + source_r*blend_r));
            blend.g = (uint8_t)(255.0f*((1.0f - source_g)*dest_g + source_g*blend_g));
            blend.b = (uint8_t)(255.0f*((1.0f - source_b)*dest_b + source_b*blend_b));

            *dest_pixel++ = blend;
        }
        source_row += source->pitch;
        dest_row   += dest->pitch;
    }
}

function void
BlitBitmapMask(Bitmap *dest, Bitmap *source, V2i p, Color foreground, Color background)
{
    int64_t source_min_x = Max(0, -p.x);
    int64_t source_min_y = Max(0, -p.y);
    int64_t source_max_x = Min(source->w, dest->w - p.x);
    int64_t source_max_y = Min(source->h, dest->h - p.y);
    int64_t source_adjusted_w = source_max_x - source_min_x;
    int64_t source_adjusted_h = source_max_y - source_min_y;

    p = Clamp(p, MakeV2i(0, 0), MakeV2i(dest->w, dest->h));

    Color *source_row = source->data + source_min_y*source->pitch + source_min_x;
    Color *dest_row = dest->data + p.y*dest->pitch + p.x;
    for (int64_t y = 0; y < source_adjusted_h; ++y)
    {
        Color *source_pixel = source_row;
        Color *dest_pixel = dest_row;
        for (int64_t x = 0; x < source_adjusted_w; ++x)
        {
            Color color = *source_pixel++;
            if (color.a)
            {
                *dest_pixel = foreground;
            }
            else
            {
                *dest_pixel = background;
            }
            ++dest_pixel;
        }
        source_row += source->pitch;
        dest_row += dest->pitch;
    }
}

function Bitmap
MakeBitmapView(Bitmap *source, Rect2i rect)
{
    rect = Intersect(rect, 0, 0, source->w, source->h);

    Bitmap result = {};
    result.w = SafeTruncateI64(GetWidth(rect));
    result.h = SafeTruncateI64(GetHeight(rect));
    result.pitch = source->pitch;
    result.data = source->data + source->pitch*rect.min.y + rect.min.x;
    return result;
}

function Rect2i
GetGlyphRect(Font *font, Glyph glyph)
{
    AssertSlow(glyph < font->glyph_count);
    uint32_t glyph_x = font->glyph_w*(glyph % font->glyphs_per_row);
    uint32_t glyph_y = font->glyph_h*(glyph / font->glyphs_per_row);
    Rect2i result = MakeRect2iMinDim(glyph_x, glyph_y, font->glyph_w, font->glyph_h);
    return result;
}

function void
BlitCharMask(Bitmap *dest, Font *font, V2i p, Glyph glyph, Color foreground, Color background)
{
    if (glyph < font->glyph_count)
    {
        Bitmap glyph_bitmap = MakeBitmapView(&font->bitmap, GetGlyphRect(font, glyph));
        BlitBitmapMask(dest, &glyph_bitmap, p, foreground, background);
    }
}

function RenderCommand *
PushRenderCommand(RenderCommandKind kind)
{
    Assert(!render_state->there_is_an_unhashed_render_command);

    RenderCommand *command = &render_state->null_command;

    size_t size_required = sizeof(RenderCommand) + sizeof(RenderSortKey);
    size_t size_left     = render_state->cb_sort_key_at - render_state->cb_command_at;

    if (size_left >= size_required)
    {
        render_state->cb_sort_key_at -= sizeof(RenderSortKey);
        RenderSortKey *sort_key = (RenderSortKey *)(render_state->command_buffer + render_state->cb_sort_key_at);

        uint32_t offset = render_state->cb_command_at;
        command = (RenderCommand *)(render_state->command_buffer + offset);
        render_state->cb_command_at += sizeof(RenderCommand);

        ZeroStruct(command);

        command->clip_rect = render_state->current_clip_rect;
        command->kind = kind;
        command->cached_cleartype = core_config->use_cached_cleartype_blend;

        sort_key->offset = offset;
        sort_key->layer  = render_state->current_layer;
    }
    else
    {
        INVALID_CODE_PATH;
    }

    render_state->there_is_an_unhashed_render_command = true;

    return command;
}

function void
HashRenderCommandInternal(RenderCommand *command, View *view, Rect2i overlapping_rect)
{
    RenderCommand to_hash = *command;
    to_hash.utf8.data = nullptr;

    int64_t line_count = GetHeight(view->viewport);

    int64_t min_line = overlapping_rect.min.y - view->viewport.min.y;
    int64_t max_line = overlapping_rect.max.y - view->viewport.min.y;

    min_line = Clamp(min_line, 0, line_count);
    max_line = Clamp(max_line, 0, line_count);

    for (int64_t line = min_line; line < max_line; line += 1)
    {
        HashResult hash = {};
        hash.u64[0] = view->line_hashes[line];
        hash = HashData(hash, sizeof(to_hash), &to_hash);
        hash = HashString(hash, command->utf8);
        view->line_hashes[line] = hash.u64[0];
    }
}

function void
HashRenderCommand(RenderCommand *command, Rect2i overlapping_rect)
{
    Assert(render_state->there_is_an_unhashed_render_command);

    RenderClipRect *clip_rect = render_state->clip_rects[render_state->current_clip_rect];
    if (clip_rect->view)
    {
        View *view = GetView(clip_rect->view);
        HashRenderCommandInternal(command, view, overlapping_rect);
    }
    else if (render_state->current_layer == Layer_OverlayBackground ||
             render_state->current_layer == Layer_OverlayForeground)
    {
        for (ViewIterator it = IterateViews(); IsValid(&it); Next(&it))
        {
            View *view = it.view;
            if (RectanglesOverlap(overlapping_rect, view->viewport))
            {
                HashRenderCommandInternal(command, view, overlapping_rect);
            }
        }
    }

    render_state->there_is_an_unhashed_render_command = false;
}

function void
PushRect(const Rect2i &rect, Color color)
{
    RenderCommand *command = PushRenderCommand(RenderCommand_Rect);
    command->rect       = rect;
    command->foreground = color;
    HashRenderCommand(command, command->rect);
}

function void
PushTile(V2i tile_p, Sprite sprite, TextStyleFlags flags = 0)
{
    RenderCommand *command = PushRenderCommand(RenderCommand_Sprite);
    command->p          = tile_p;
    command->font       = editor->fonts[flags];
    command->glyph      = sprite.glyph;
    command->foreground = sprite.foreground;
    command->background = sprite.background;
    command->rect.min   = tile_p;
    command->rect.max   = tile_p + MakeV2i(1, 1);
    HashRenderCommand(command, command->rect);
}

function void
PushUnicode(V2i tile_p, String utf8, Color foreground, Color background, TextStyleFlags flags = 0)
{
    RenderCommand *command = PushRenderCommand(RenderCommand_Unicode);
    command->p          = tile_p;
    command->font       = editor->fonts[flags];
    command->utf8       = utf8;
    command->foreground = foreground;
    command->background = background;
    command->rect.min   = tile_p;
    command->rect.max   = tile_p + MakeV2i(1, 1);
    HashRenderCommand(command, command->rect);
}

function void
PushRectOutline(const Rect2i &rect, Color foreground, Color background)
{
    uint32_t left   = Wall_Left;
    uint32_t right  = Wall_Right;
    uint32_t top    = Wall_Top;
    uint32_t bottom = Wall_Bottom;

    PushTile(rect.min, MakeWall(right|bottom, foreground, background));
    PushTile(MakeV2i(rect.max.x - 1, rect.min.y), MakeWall(left|bottom, foreground, background));
    PushTile(rect.max - MakeV2i(1, 1), MakeWall(left|top, foreground, background));
    PushTile(MakeV2i(rect.min.x, rect.max.y - 1), MakeWall(right|top, foreground, background));

    for (int64_t x = rect.min.x + 1; x < rect.max.x - 1; ++x)
    {
        PushTile(MakeV2i(x, rect.min.y), MakeWall(left|right, foreground, background));
        PushTile(MakeV2i(x, rect.max.y - 1), MakeWall(left|right, foreground, background));
    }

    for (int64_t y = rect.min.y + 1; y < rect.max.y - 1; ++y)
    {
        PushTile(MakeV2i(rect.min.x, y), MakeWall(top|bottom, foreground, background));
        PushTile(MakeV2i(rect.max.x - 1, y), MakeWall(top|bottom, foreground, background));
    }
}

function void
PushBitmap(Bitmap *bitmap, V2i p)
{
    RenderCommand *command = PushRenderCommand(RenderCommand_Bitmap);
    command->p      = p;
    command->rect   = MakeRect2iMinDim(p.x, p.y, bitmap->w, bitmap->h);
    command->bitmap = bitmap;

    // TODO: It's time to stop assuming rendering stuff is scaled by the font size implicitly in this renderer
    int64_t snapped_x = p.x / editor->font_metrics.x;
    int64_t snapped_y = p.y / editor->font_metrics.y;
    int64_t snapped_w = (bitmap->w + editor->font_metrics.x - 1) / editor->font_metrics.x;
    int64_t snapped_h = (bitmap->h + editor->font_metrics.y - 1) / editor->font_metrics.y;

    HashRenderCommand(command, MakeRect2iMinDim(snapped_x, snapped_y, snapped_w, snapped_h));
}

function uint16_t
PushClipRect(Rect2i rect, ViewID view)
{
    uint16_t result = render_state->current_clip_rect;
    if (render_state->clip_rect_count < ArrayCount(render_state->clip_rects))
    {
        render_state->current_clip_rect = render_state->clip_rect_count++;
        RenderClipRect *clip_rect = PushStruct(render_state->arena, RenderClipRect);
        render_state->clip_rects[render_state->current_clip_rect] = clip_rect;
        clip_rect->view = view;
        clip_rect->rect = rect;
    }
    return result;
}

function RenderLayer
PushLayer(RenderLayer layer)
{
    RenderLayer result = render_state->current_layer;
    render_state->current_layer = layer;
    return result;
}

static void
BeginRender(void)
{
    render_state->arena_index = !render_state->arena_index;
    render_state->arena = &render_state->arenas[render_state->arena_index];
    Clear(render_state->arena);

    Bitmap *target = render_state->target;
    int64_t viewport_w = target->w / editor->font_metrics.x;
    int64_t viewport_h = target->h / editor->font_metrics.y;
    render_state->viewport = MakeRect2iMinDim(0, 0, viewport_w, viewport_h);

    render_state->cb_command_at = 0;
    render_state->cb_sort_key_at = render_state->cb_size;

    render_state->clip_rect_count = 0;
    Rect2i clip_rect = render_state->viewport;
    clip_rect.max.x += 1;
    clip_rect.max.y += 1;
    PushClipRect(Scale(clip_rect, editor->font_metrics));
}

function void
MergeSortInternal(uint32_t count, uint32_t *a, uint32_t *b)
{
    if (count <= 1)
    {
        // Nothing to do
    }
    else
    {
        uint32_t count_l = count / 2;
        uint32_t count_r = count - count_l;
        
        MergeSortInternal(count_l, b, a);
        MergeSortInternal(count_r, b + count_l, a + count_l);
        
        uint32_t *middle = b + count_l;
        uint32_t *end    = b + count;
        
        uint32_t *l = b;
        uint32_t *r = b + count_l;
        
        uint32_t *out = a;
        for (size_t i = 0; i < count; ++i)
        {
            if ((l < middle) &&
                ((r >= end) || (*l <= *r)))
            {
                *out++ = *l++;
            }
            else
            {
                *out++ = *r++;
            }
        }
    }
}

function void
MergeSort(uint32_t count, uint32_t *a, uint32_t *b)
{
    CopyArray(count, a, b);
    MergeSortInternal(count, a, b);
}

function void
RasterizeText(PlatformOffscreenBuffer *buffer, Rect2i rect, PlatformFontHandle font, String text, Color foreground, Color background)
{
    platform->SetTextClipRect(buffer, rect);
    platform->DrawText(font, buffer, text, rect.min, foreground, background);
}

function void
RasterizeTextCachedCleartypeBlend(PlatformOffscreenBuffer *buffer, Rect2i rect, PlatformFontHandle font, String text, Color foreground, Color background)
{
    BlitRect(&buffer->bitmap, rect, background);

    platform->SetTextClipRect(buffer, rect);
    platform->DrawText(font, buffer, text, rect.min, foreground, background);

    Color *row = buffer->bitmap.data + rect.min.y*buffer->bitmap.pitch + rect.min.x;
    for (int64_t y = rect.min.y; y < rect.max.y; y += 1)
    {
        Color *pixel = row;
        for (int64_t x = rect.min.x; x < rect.max.x; x += 1)
        {
            Color color = *pixel;
            if ((color.u32 & 0x00FFFFFF) == (background.u32 & 0x00FFFFFF))
            {
                color.u32 = 0;
            }
            else
            {
                color.a = 255;
            }
            *pixel++ = color;
        }
        row += buffer->bitmap.pitch;
    }
}

function void
RenderCommandsToBitmap(void)
{
    uint32_t sort_key_count = (render_state->cb_size - render_state->cb_sort_key_at) / sizeof(RenderSortKey);
    RenderSortKey *sort_keys = (RenderSortKey *)(render_state->command_buffer + render_state->cb_sort_key_at);
    RenderSortKey *sort_scratch = PushArrayNoClear(render_state->arena, sort_key_count, RenderSortKey);
    RadixSort(sort_key_count, &sort_keys->u32, &sort_scratch->u32);

#if TEXTIT_SLOW
    for (size_t i = 1; i < sort_key_count; ++i)
    {
        Assert(sort_keys[i].u32 > sort_keys[i - 1].u32);
    }
#endif

    V2i metrics = editor->font_metrics;
    int32_t glyphs_per_row = render_state->glyphs_per_row;

    int glyph_fills = 0;

    size_t         clip_rect_index   = SIZE_MAX;
    RenderClipRect *clip_rect        = nullptr;
    Rect2i         viewport          = {};
    int64_t        line_count        = 0;
    uint64_t       *line_hashes      = nullptr;
    uint64_t       *prev_line_hashes = nullptr;

    Bitmap target = *render_state->target;

    RenderSortKey *end = (RenderSortKey *)(render_state->command_buffer + render_state->cb_size);
    for (RenderSortKey *at = sort_keys; at < end; at += 1)
    {
        RenderCommand *command = (RenderCommand *)(render_state->command_buffer + at->offset);
        V2i p        = command->p;
        V2i screen_p = MakeV2i(metrics.x*p.x, metrics.y*p.y);

        if (command->clip_rect != clip_rect_index)
        {
            clip_rect_index = command->clip_rect;
            clip_rect = render_state->clip_rects[clip_rect_index];

            View *view = GetView(clip_rect->view);
            viewport         = view->viewport;
            line_count       = GetHeight(viewport);
            line_hashes      = view->line_hashes;
            prev_line_hashes = view->prev_line_hashes;

            target = MakeBitmapView(render_state->target, clip_rect->rect);
        }

        if (clip_rect->view)
        {
            int64_t min_line = command->rect.min.y - viewport.min.y;
            int64_t max_line = command->rect.max.y - viewport.min.y;

            min_line = Clamp(min_line, 0, line_count);
            max_line = Clamp(max_line, 0, line_count);

            bool all_hashes_match = true;
            for (int64_t line = min_line; line < max_line; line += 1)
            {
                if (line_hashes[line] != prev_line_hashes[line])
                {
                    all_hashes_match = false;
                    break;
                }
            }

            if (all_hashes_match)
            {
                continue;
            }
        }

        switch (command->kind)
        {
            case RenderCommand_Sprite:
            case RenderCommand_Unicode:
            {
                Rect2i glyph_rect = MakeRect2iMinDim(screen_p, MakeV2i(metrics.x, metrics.y));
                if (RectanglesOverlap(clip_rect->rect, glyph_rect))
                {
                    uint8_t glyph = (uint8_t)command->glyph;
                    String text = (command->kind == RenderCommand_Unicode ? command->utf8 : MakeString(1, &glyph));

                    PlatformFontHandle font = command->font;

                    glyph_rect.min -= clip_rect->rect.min;
                    glyph_rect.max -= clip_rect->rect.min;

                    if (text.size == 1 && text.data[0] == ' ')
                    {
                        // nothing lol
                    }
                    else
                    {
                        HashResult hash = {};
                        if (core_config->use_cached_cleartype_blend)
                        {
                            hash = HashIntegers(command->foreground.u32, command->background.u32);
                        }
                        hash = HashIntegers(hash, (uint64_t)font);
                        hash = HashString(hash, text);

                        GlyphEntry *entry = GetGlyphEntry(&render_state->glyph_cache, hash);

                        V2i glyph_p = MakeV2i(editor->font_max_glyph_size.x*(entry->index % glyphs_per_row),
                                              editor->font_max_glyph_size.y*(entry->index / glyphs_per_row));

                        Rect2i rasterize_rect = MakeRect2iMinDim(glyph_p, editor->font_max_glyph_size);
                        Bitmap glyph_bitmap = MakeBitmapView(&render_state->glyph_texture.bitmap, rasterize_rect);
                        if (entry->state == GlyphState_Empty)
                        {
                            if (core_config->use_cached_cleartype_blend)
                            {
                                RasterizeTextCachedCleartypeBlend(&render_state->glyph_texture, rasterize_rect, font, text, command->foreground, command->background);
                            }
                            else
                            {
                                BlitRect(&render_state->glyph_texture.bitmap, rasterize_rect, COLOR_WHITE);
                                RasterizeText(&render_state->glyph_texture, rasterize_rect, font, text, COLOR_BLACK, COLOR_WHITE);
                            }
                            entry->state = GlyphState_Filled;
                            glyph_fills += 1;
                        }
                        if (core_config->use_cached_cleartype_blend)
                        {
                            BlitBitmapAlphaMasked(&target, &glyph_bitmap, glyph_rect.min);
                        }
                        else
                        {
                            BlitBitmapSubpixelBlend(&target, &glyph_bitmap, glyph_rect.min, command->foreground);
                        }
                    }
                }
            } break;

            case RenderCommand_Rect:
            {
                Rect2i rect = command->rect;

                rect.min *= metrics;
                rect.max *= metrics;

                if (RectanglesOverlap(clip_rect->rect, rect))
                {
                    rect.min -= clip_rect->rect.min;
                    rect.max -= clip_rect->rect.min;

                    BlitRect(&target, rect, command->foreground);
                }
            } break;

            case RenderCommand_Bitmap:
            {
                Rect2i rect = command->rect;

                if (RectanglesOverlap(clip_rect->rect, rect))
                {
                    rect.min -= clip_rect->rect.min;
                    rect.max -= clip_rect->rect.min;

                    BlitBitmap(&target, command->bitmap, p);
                }
            } break;
        }
    }

    if (glyph_fills > 0)
    {
        platform->DebugPrint("glyph fills: %d\n", glyph_fills);
    }
}

static void
EndRender(void)
{
    RenderCommandsToBitmap();
}
