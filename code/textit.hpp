#ifndef TEXTIT_HPP
#define TEXTIT_HPP

#include <stdarg.h>
#include <stdio.h>

#include "textit_platform.hpp"
#include "textit_intrinsics.hpp"
#include "textit_shared.hpp"
#include "textit_memory.hpp"
#include "textit_global_state.hpp"
#include "textit_math.hpp"
#include "textit_random.hpp"
#include "textit_image.hpp"
#include "textit_render.hpp"

#define TEXTIT_BUFFER_SIZE Megabytes(1)
struct Buffer
{
    Arena arena;
    String name;

    int32_t count;
    char text[TEXTIT_BUFFER_SIZE];
};

struct View
{
    Arena arena;
    Buffer *buffer;
    V2i cursor;
};

struct BufferLocation
{
    int32_t line_number;
    Range line_range;
    int32_t pos;
};

struct EditorState
{
    GlobalState global_state;

    Arena permanent_arena;
    Arena transient_arena;

    Font font;

    Buffer *open_buffer;
    View *open_view;

    V2i screen_mouse_p;
    V2i text_mouse_p;

    int debug_delay;
    int debug_delay_frame_count;
};
static EditorState *editor_state;

static inline void SetDebugDelay(int milliseconds, int frame_count);

#endif /* TEXTIT_HPP */
