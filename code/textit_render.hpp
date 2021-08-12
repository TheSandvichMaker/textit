#ifndef TEXTIT_RENDER_HPP
#define TEXTIT_RENDER_HPP

// NOTE: These are for code page 437 fonts
// https://en.wikipedia.org/wiki/Code_page_437#Character_set
typedef uint32_t Glyph;
enum
{
    Glyph_Empty                  = 0,
    Glyph_Dwarf1                 = 1,
    Glyph_Dwarf2                 = 2,
    Glyph_Heart                  = 3,
    Glyph_Diamond                = 4,
    Glyph_Club                   = 5,
    Glyph_Spade                  = 6,
    Glyph_Circle                 = 7,
    Glyph_CircleNegative         = 8,
    Glyph_CircleOutline          = 9,
    Glyph_CircleOutlineNegative  = 10,
    Glyph_Male                   = 11,
    Glyph_Female                 = 12,
    Glyph_MusicNote              = 13,
    Glyph_MusicNotes             = 14,
    Glyph_Sun                    = 15,
    Glyph_TriangleRight          = 16,
    Glyph_TriangeLeft            = 17,
    Glyph_ArrowsUpDown           = 18,
    Glyph_DoubleExclamation      = 19,
    Glyph_Paragraph              = 20,
    Glyph_Section                = 21,
    Glyph_LowBar                 = 22,
    Glyph_ArrowsUpDownUnderlined = 23,
    Glyph_ArrowUp                = 24,
    Glyph_ArrowDown              = 25,
    Glyph_ArrowLeft              = 26,
    Glyph_ArrowRight             = 27,
    Glyph_RightAngle             = 28, // TODO: What's this?
    Glyph_ArrowsLeftRight        = 29,
    Glyph_TriangleUp             = 30,
    Glyph_TriangleDown           = 31,
    /* 32 ... 126                = ascii... kinda */
    Glyph_Cent                   = 155,
    Glyph_Pound                  = 156,
    Glyph_Yen                    = 157,
    Glyph_Peseta                 = 158,
    Glyph_HookedF                = 159,
    Glyph_InvertedQuestion       = 160,
    Glyph_Half                   = 171,
    Glyph_Quarter                = 172,
    Glyph_InvertedExclamation    = 173,
    Glyph_DoubleChevronLeft      = 174,
    Glyph_DoubleChevronRight     = 175,
    Glyph_Tone25                 = 176,
    Glyph_Tone50                 = 177,
    Glyph_Tone75                 = 178,
    /* 179 ... 223               = Box drawing characters */
    Glyph_Solid                  = 219,
    Glyph_Alpha                  = 224,
    Glyph_Beta                   = 225,
    Glyph_Gamma                  = 226,
    Glyph_Pi                     = 227,
    Glyph_SigmaUpper             = 228,
    Glyph_SigmaLower             = 229,
    Glyph_Mu                     = 230,
    Glyph_Tau                    = 231,
    Glyph_Phi                    = 232,
    Glyph_Theta                  = 233,
    Glyph_Omega                  = 234,
    Glyph_Delta                  = 235,
    Glyph_Infinity               = 236,
    Glyph_Finite                 = 237,
    Glyph_ElementOf              = 238,
    Glyph_Intersection           = 239,
    Glyph_TripleBar              = 240,
    Glyph_PlusMinus              = 241,
    Glyph_GreaterOrEquals        = 242,
    Glyph_LessOrEquals           = 243,
    Glyph_IntegralUpper          = 244,
    Glyph_IntegralLower          = 245,
    Glyph_Division               = 246,
    Glyph_ApproxEquals           = 247,
    Glyph_DegreeSymbol           = 248,
    Glyph_Bullet                 = 249,
    Glyph_Interpunct             = 250,
    Glyph_SquareRoot             = 251,
    Glyph_SuperscriptN           = 252,
    Glyph_Superscript2           = 253,
    Glyph_Block                  = 254,
    Glyph_NonbreakingSpace       = 255,
};

struct Sprite
{
    Glyph glyph;
    Color foreground;
    Color background;
};

static inline Sprite
MakeSprite(Glyph glyph, Color foreground = COLOR_WHITE, Color background = COLOR_BLACK)
{
    Sprite sprite = {};
    sprite.glyph = glyph;
    sprite.foreground = foreground;
    sprite.background = background;
    return sprite;
}

enum WallSegment
{
    Wall_Left        = 0x1,
    Wall_LeftThick   = 0x2,
    Wall_Right       = 0x4,
    Wall_RightThick  = 0x8,
    Wall_Top         = 0x10,
    Wall_TopThick    = 0x20,
    Wall_Bottom      = 0x40,
    Wall_BottomThick = 0x80,
    Wall_MAXVALUE = Wall_LeftThick|Wall_RightThick|Wall_TopThick|Wall_BottomThick,
};

enum RenderLayer
{
    Layer_ViewBackground,
    Layer_ViewForeground,
    Layer_OverlayBackground,
    Layer_OverlayForeground,
    Layer_COUNT,
};

enum RenderCommandKind : uint16_t
{
    RenderCommand_Sprite,
    RenderCommand_Rect,
    RenderCommand_Unicode,
    RenderCommand_Bitmap,
};

union RenderSortKey
{
    struct
    {
        uint32_t offset : 30;
        uint32_t layer  : 2;
    };
    uint32_t u32;
};

struct RenderCommand
{
    uint16_t clip_rect;
    RenderCommandKind kind;

    Rect2i rect;

    V2i p;

    Color foreground;
    Color background;

    String utf8;
    Glyph glyph;
    Bitmap *bitmap;
};

struct RenderClipRect
{
    ViewID view;
    Rect2i rect;
};

struct RenderState
{
    int arena_index;
    Arena arenas[2];
    Arena *arena;

    Bitmap *target;

    GlyphCache glyph_cache;
    uint32_t glyphs_per_row, glyphs_per_col;
    PlatformOffscreenBuffer glyph_texture;

    RenderLayer current_layer;

    Rect2i viewport;

    Glyph wall_segment_lookup[Wall_MAXVALUE + 1];

    uint64_t command_buffer_hash;
    uint64_t prev_command_buffer_hash;

    RenderCommand null_command;
    bool there_is_an_unhashed_render_command;
    uint32_t cb_size;
    uint32_t cb_command_at;
    uint32_t cb_sort_key_at;
    char *command_buffer;

    uint16_t current_clip_rect;
    uint16_t clip_rect_count;
    RenderClipRect *clip_rects[512];
};
GLOBAL_STATE(RenderState, render_state);

function uint16_t PushClipRect(Rect2i rect, ViewID view = {});

struct ScopedClipRect
{
    uint16_t old_index;
    ScopedClipRect(Rect2i rect, ViewID view = {})
    {
        old_index = PushClipRect(rect, view);
    }
    ~ScopedClipRect()
    {
        render_state->current_clip_rect = old_index;
    }
};
#define PushScopedClipRect(rect) ScopedClipRect Paste(scoped_clip_rect_, __LINE__)

struct ScopedRenderLayer
{
    RenderLayer old_layer;
    ScopedRenderLayer(RenderLayer new_layer)
    {
        old_layer = render_state->current_layer;
        render_state->current_layer = new_layer;
    }
    ~ScopedRenderLayer()
    {
        render_state->current_layer = old_layer;
    }
};
#define PushScopedRenderLayer(layer) ScopedRenderLayer Paste(scoped_render_layer, __LINE__)

#endif /* TEXTIT_RENDER_HPP */
