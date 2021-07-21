function void
InitializeRenderState(Arena *arena, Bitmap *target, Font *font)
{
    render_state->target     = target;
    render_state->fonts[Layer_Background] = font;
    render_state->fonts[Layer_Text]       = font;
    render_state->fonts[Layer_Overlay]    = font;

    render_state->cb_size = Megabytes(4); // random choice
    render_state->command_buffer = PushArrayNoClear(arena, render_state->cb_size, char);

    int64_t viewport_w = (target->w + font->glyph_w - 1) / font->glyph_w;
    int64_t viewport_h = (target->h + font->glyph_h - 1) / font->glyph_h;
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

function V2i
GlyphDim(Font *font)
{
    return MakeV2i(font->glyph_w, font->glyph_h);
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
    uint32_t glyph_y = font->glyph_h*(glyph / font->glyphs_per_col);
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
PushRenderCommand(RenderLayer layer, RenderCommandKind kind)
{
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

        command->kind = kind;

        sort_key->offset = offset;
        sort_key->layer  = layer;
    }
    else
    {
        INVALID_CODE_PATH;
    }

    return command;
}

function void
EndPushRenderCommand(RenderCommand *command)
{
    if (command != &render_state->null_command)
    {
        // render_state->command_buffer_hash = HashData(render_state->command_buffer_hash, sizeof(*sort_key), sort_key);
        // render_state->command_buffer_hash = HashData(render_state->command_buffer_hash, sizeof(*command), command);
    }
}

function void
PushTile(RenderLayer layer, V2i tile_p, Sprite sprite)
{
    RenderCommand *command = PushRenderCommand(layer, RenderCommand_Sprite);
    command->p = tile_p;
    command->sprite = sprite;
}

function void
PushText(RenderLayer layer, V2i p, String text, Color foreground, Color background)
{
    V2i at = p;

    Sprite sprite = {};
    sprite.foreground = foreground;
    sprite.background = background;

    for (size_t i = 0; i < text.size; ++i)
    {
        sprite.glyph = text.data[i];
        PushTile(layer, at, sprite);
        at.x += 1;
    }
}

function void
PushRect(RenderLayer layer, const Rect2i &rect, Color color)
{
    RenderCommand *command = PushRenderCommand(layer, RenderCommand_Rect);
    command->rect = rect;
    command->color = color;
}

function void
PushRectOutline(RenderLayer layer, const Rect2i &rect, Color foreground, Color background)
{
    uint32_t left   = Wall_Left;
    uint32_t right  = Wall_Right;
    uint32_t top    = Wall_Top;
    uint32_t bottom = Wall_Bottom;
#if 0
    if (flags & Format_OutlineThickHorz)
    {
        left  = Wall_LeftThick;
        right = Wall_RightThick;
    }
    if (flags & Format_OutlineThickVert)
    {
        top    = Wall_TopThick;
        bottom = Wall_BottomThick;
    }
#endif

    PushRect(layer, MakeRect2iMinMax(rect.min + MakeV2i(1, 1), rect.max - MakeV2i(1, 1)), background);

    PushTile(layer, rect.min, MakeWall(right|bottom, foreground, background));
    PushTile(layer, MakeV2i(rect.max.x - 1, rect.min.y), MakeWall(left|bottom, foreground, background));
    PushTile(layer, rect.max - MakeV2i(1, 1), MakeWall(left|top, foreground, background));
    PushTile(layer, MakeV2i(rect.min.x, rect.max.y - 1), MakeWall(right|top, foreground, background));

    for (int64_t x = rect.min.x + 1; x < rect.max.x - 1; ++x)
    {
        PushTile(layer, MakeV2i(x, rect.min.y), MakeWall(left|right, foreground, background));
        PushTile(layer, MakeV2i(x, rect.max.y - 1), MakeWall(left|right, foreground, background));
    }

    for (int64_t y = rect.min.y + 1; y < rect.max.y - 1; ++y)
    {
        PushTile(layer, MakeV2i(rect.min.x, y), MakeWall(top|bottom, foreground, background));
        PushTile(layer, MakeV2i(rect.max.x - 1, y), MakeWall(top|bottom, foreground, background));
    }
}

static void
BeginRender(void)
{
    Arena *arena = platform->GetTempArena();
    render_state->arena = arena;

    Bitmap *target = render_state->target;
    Font *font = render_state->fonts[Layer_Text];
    int64_t viewport_w = (target->w) / font->glyph_w;
    int64_t viewport_h = (target->h) / font->glyph_h;
    render_state->viewport = MakeRect2iMinDim(0, 0, viewport_w, viewport_h);

    render_state->cb_command_at = 0;
    render_state->cb_sort_key_at = render_state->cb_size;

    Swap(render_state->command_buffer_hash, render_state->prev_command_buffer_hash);
    render_state->command_buffer_hash = 0;
}

struct TiledRenderJobParams
{
    Rect2i clip_rect;
    Bitmap *target;
};

static
PLATFORM_JOB(TiledRenderJob)
{
    TiledRenderJobParams *params = (TiledRenderJobParams *)userdata;

    Rect2i clip_rect = params->clip_rect;

    Bitmap target = MakeBitmapView(params->target, clip_rect);
    ClearBitmap(&target);

    char *command_buffer = render_state->command_buffer;
    RenderSortKey *sort_keys = (RenderSortKey *)(command_buffer + render_state->cb_sort_key_at);

    RenderSortKey *end = (RenderSortKey *)(command_buffer + render_state->cb_size);
    for (RenderSortKey *at = sort_keys; at < end; at += 1)
    {
        RenderCommand *command = (RenderCommand *)(command_buffer + at->offset);
        Font *font = render_state->fonts[at->layer];
        V2i glyph_dim = GlyphDim(font);

        switch (command->kind)
        {
            case RenderCommand_Sprite:
            {
                V2i p = MakeV2i(command->p.x, command->p.y);
                Sprite *sprite = &command->sprite;

                if (sprite->glyph == '@')
                {
                    int y = 0; (void)y;
                }

                p *= glyph_dim;
                if (RectanglesOverlap(clip_rect, MakeRect2iMinDim(p, MakeV2i(font->glyph_w, font->glyph_h))))
                {
                    p -= clip_rect.min;

                    Rect2i glyph_rect = GetGlyphRect(font, sprite->glyph);
                    Bitmap glyph_bitmap = MakeBitmapView(&font->bitmap, glyph_rect);
                    BlitBitmapMask(&target, &glyph_bitmap, p, sprite->foreground, sprite->background);
                }
            } break;

            case RenderCommand_Rect:
            {
                Rect2i rect = command->rect;

                rect.min *= glyph_dim;
                rect.max *= glyph_dim;

                if (RectanglesOverlap(clip_rect, rect))
                {
                    rect.min -= clip_rect.min;
                    rect.max -= clip_rect.min;

                    BlitRect(&target, rect, command->color);
                }
            } break;
        }
    }
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
RadixSort(uint32_t count, uint32_t *data, uint32_t *temp)
{
    uint32_t *source = data;
    uint32_t *dest = temp;

    for (int byte_index = 0; byte_index < 4; ++byte_index)
    {
        uint32_t offsets[256] = {};

        // NOTE: First pass - count how many of each key
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i] >> 8*byte_index) & 0xFF;
            offsets[radix_piece] += 1;
        }

        // NOTE: Change counts to offsets
        uint32_t total = 0;
        for (size_t i = 0; i < ArrayCount(offsets); ++i)
        {
            uint32_t piece_count = offsets[i];
            offsets[i] = total;
            total += piece_count;
        }

        // NOTE: Second pass - place elements into dest in order
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t radix_piece = (source[i] >> 8*byte_index) & 0xFF;
            dest[offsets[radix_piece]++] = source[i];
        }

        Swap(dest, source);
    }
}

function void
RenderCommandsToBitmap(Bitmap *target)
{
    Rect2i target_bounds = MakeRect2iMinDim(0, 0, target->w, target->h);

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

    int tile_count_x = 8;
    int tile_count_y = 8;
    int tile_count = tile_count_x*tile_count_y;
    TiledRenderJobParams *tiles = PushArray(render_state->arena, tile_count, TiledRenderJobParams);

    int tile_w = (target->w + tile_count_x - 1) / tile_count_x;
    int tile_h = (target->h + tile_count_y - 1) / tile_count_y;

    for (int tile_y = 0; tile_y < tile_count_y; ++tile_y)
    for (int tile_x = 0; tile_x < tile_count_x; ++tile_x)
    {
        int tile_index = tile_y*tile_count_x + tile_x;

        TiledRenderJobParams *params = &tiles[tile_index];
        params->target = target;

        Rect2i clip_rect = MakeRect2iMinDim(tile_x*tile_w, tile_y*tile_h, tile_w, tile_h);
        clip_rect = Intersect(clip_rect, target_bounds); 
        params->clip_rect = clip_rect;

        platform->AddJob(platform->high_priority_queue, params, TiledRenderJob);
    }

    platform->WaitForJobs(platform->high_priority_queue);
}

static void
EndRender(void)
{
    // if (render_state->command_buffer_hash != render_state->prev_command_buffer_hash)
    {
        RenderCommandsToBitmap(render_state->target);
    }
}

function void
PrintRenderCommandsUnderCursor(void)
{
    char *command_buffer = render_state->command_buffer;
    RenderSortKey *sort_keys = (RenderSortKey *)(command_buffer + render_state->cb_sort_key_at);

    RenderSortKey *end = (RenderSortKey *)(command_buffer + render_state->cb_size);
    int index = 0;
    for (RenderSortKey *at = sort_keys; at < end; at += 1)
    {
        RenderCommand *command = (RenderCommand *)(command_buffer + at->offset);

        Font *font = render_state->fonts[at->layer];
        V2i glyph_dim = GlyphDim(font);

        char *type = "Huh";
        Rect2i surface_area = {};
        switch (command->kind)
        {
            case RenderCommand_Sprite:
            {
                type = "sprite";

                V2i p = MakeV2i(command->p.x, command->p.y);

                p *= glyph_dim;
                surface_area = MakeRect2iMinDim(p, glyph_dim);
            } break;

            case RenderCommand_Rect:
            {
                type = "rect";

                Rect2i rect = command->rect;

                rect.min *= glyph_dim;
                rect.max *= glyph_dim;
                surface_area = rect;
            } break;
        }

        if (IsInRect(surface_area, editor_state->screen_mouse_p))
        {
            platform->DebugPrint("type: %s, index: %d\n", type, index);
        }

        ++index;
    }
}
