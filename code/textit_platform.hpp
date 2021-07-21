#ifndef TEXTIT_PLATFORM_HPP
#define TEXTIT_PLATFORM_HPP

#if TEXTIT_BUILD_DLL
#define TEXTIT_EXPORT __declspec(dllexport)
#else
#define TEXTIT_EXPORT
#endif

#define SimpleAssert(x) ((x) ? 1 : (__debugbreak(), 0))
#define Assert(x) \
    ((x) ? 1 \
         : (platform->ReportError(PlatformError_Fatal, \
                                  "Assertion Failed: " #x " at file %s, line %d", __FILE__, __LINE__), 0))

// should this be inline or static inline
#define TEXTIT_INLINE inline

#ifdef DEBUG_BUILD
#define AssertSlow(x) Assert(x)
#else
#define AssertSlow(x) 
#endif

#define StaticAssert(condition, message) static_assert(condition, message)

#define UNUSED_VARIABLE(x) (void)(x)

#define FILE_AND_LINE_STRING__(File, Line) File ":" #Line
#define FILE_AND_LINE_STRING_(File, Line) FILE_AND_LINE_STRING__(File, Line)
#define FILE_AND_LINE_STRING FILE_AND_LINE_STRING_(__FILE__, __LINE__)
#define LOCATION_STRING(...) FILE_AND_LINE_STRING " (" __VA_ARGS__ ")"

#define INVALID_CODE_PATH Assert(!"Invalid Code Path!");
#define INVALID_DEFAULT_CASE default: { Assert(!"Invalid Default Case!"); } break;
#define INCOMPLETE_SWITCH default: { /* nah */ } break;

#define Swap(a, b) do { auto swap_temp_ = a; a = b; b = swap_temp_; } while(0)

#define Paste__(a, b) a##b
#define Paste_(a, b) Paste__(a, b)
#define Paste(a, b) Paste_(a, b)
#define Stringize__(x) #x
#define Stringize_(x) Stringize__(x)
#define Stringize(x) Stringize_(x)
#define Expand_(x) x
#define Expand(x) Expand(x)

#define HasFlag(mask, flag) (!!((mask) & (flag)))

#define BitIsSet(mask, bit) ((mask) & ((u64)1 << bit))
#define SetBit(mask, bit)   ((mask) |= ((u64)1 << bit))
#define UnsetBit(mask, bit) ((mask) &= ~((u64)1 << bit))

#define AlignPow2(value, align) (((value) + ((align) - 1)) & ~((align) - 1))
#define Align4(value) ((value + 3) & ~3)
#define Align8(value) ((value + 7) & ~7)
#define Align16(value) ((value + 15) & ~15)

#define Kilobytes(x) ((x)*1024ull)
#define Megabytes(x) (Kilobytes(x)*1024ull)
#define Gigabytes(x) (Megabytes(x)*1024ull)
#define Terabytes(x) (Gigabytes(x)*1024ull)

#define SllStackPush(h, n) ((n)->next = (h), (h) = (n))
#define SllStackPop_(h) ((h) = (h)->next)
#define SllStackPop(h) h; SllStackPop_(h)
#define SllQueuePush(f, l, n) ((n)->next = 0, ((f) ? (l)->next = (n) : (f) = (n)), (l) = (n))
#define SllQueuePop(f, l) f; (SllStackPop_(f), ((f) ? 0 : (l) = 0))

#define ForSllUnique(it, head, next) for (auto* it = head; it; it = it->next)
#define ForSll(it, head) ForSllUnique(it, head, next)
#define ForSllOuterUnique(it_at, head, next) for (auto** it_at = &(head); *it_at; it_at = &(*it_at)->next)
#define ForSllOuter(it_at, head) ForSllOuterUnique(it_at, head, next)

#define DllInit(s) ((s)->next = s, (s)->prev = s)
#define DllInsertFront(h, n) ((n)->next = (h)->next, (n)->prev = h, (n)->next->prev = n, (n)->prev->next = n)
#define DllInsertBack(h, n) ((n)->next = h, (n)->prev = (h)->prev, (n)->next->prev = n, (n)->prev->next = n)
#define DllRemove(n) ((n)->next->prev = (n)->prev, (n)->prev->next = (n)->next)
#define DllIsEmpty(s) ((s)->next == (s))

#define ForDllUnique(it, sentinel, next, prev) for (auto prev_##it = (sentinel), it = (sentinel)->next; \
                                                    it != (sentinel);                                   \
                                                    prev_##it = prev_##it->next, it = prev_##it->next)
#define ForDll(it, sentinel) ForDllUnique(it, sentinel, next, prev)

#define IntToPointer(i) (void *)(intptr_t)(i)
#define PointerToInt(i) (intptr_t)(i)

#define function static inline
#define enum_flags(type, name) typedef type name; enum name##_ENUM : type

#include "textit_types.hpp"
#include "textit_intrinsics.hpp"

#if COMPILER_MSVC
#define _AMD64_
#include <windef.h>

#define ALWAYS_INLINE __forceinline

#define WRITE_BARRIER _WriteBarrier()
#define READ_BARRIER _ReadBarrier()

function uint32_t
AtomicAdd(volatile uint32_t *dest, uint32_t value)
{
    // NOTE: This returns the value _before_ adding
    uint32_t result = (uint32_t)_InterlockedExchangeAdd((volatile long *)dest, value);
    return result;
}

function uint32_t
AtomicIncrement(volatile uint32_t *dest)
{
    // NOTE: This returns the value _before_ adding
    uint32_t result = (uint32_t)_InterlockedIncrement((volatile long *)dest) - 1;
    return result;
}

function uint32_t
AtomicExchange(volatile int32_t *dest, int32_t value)
{
    // NOTE: This returns the value _before_ exchanging
    int32_t result = (int32_t)_InterlockedExchange((volatile long *)dest, value);
    return result;
}

function uint32_t
AtomicExchange(volatile uint32_t *dest, uint32_t value)
{
    // NOTE: This returns the value _before_ exchanging
    uint32_t result = (uint32_t)_InterlockedExchange((volatile long *)dest, value);
    return result;
}

function void *
AtomicExchange(void *volatile *dest, void *value)
{
    void *result = InterlockedExchangePointer(dest, value);
    return result;
}

function uint32_t
GetThreadID()
{
    uint8_t *thread_local_storage = (uint8_t *)__readgsqword(0x30);
    uint32_t thread_id = *(uint32_t *)(thread_local_storage + 0x48);
    return thread_id;
}
#elif COMPILER_LLVM
function uint32_t
AtomicAdd(volatile uint32_t *dest, uint32_t value)
{
    // NOTE: This returns the value _before_ adding
    uint32_t result = __atomic_fetch_add(dest, value, __ATOMIC_SEQ_CST);
    return result;
}
#endif

struct TicketMutex
{
    volatile uint32_t ticket;
    volatile uint32_t serving;
};

function void
BeginTicketMutex(TicketMutex *mutex)
{
    uint32_t ticket = AtomicAdd(&mutex->ticket, 1);
    while (ticket != mutex->serving)
    {
        _mm_pause();
    }
}

function void
EndTicketMutex(TicketMutex *mutex)
{
    AtomicAdd(&mutex->serving, 1);
}

enum PlatformEventType
{
    PlatformEvent_None,
    PlatformEvent_Any = PlatformEvent_None,
    PlatformEvent_MouseUp,
    PlatformEvent_MouseDown,
    PlatformEvent_KeyUp,
    PlatformEvent_KeyDown,
    PlatformEvent_Text,
    PlatformEvent_Tick,
    PlatformEvent_COUNT,
};

typedef uint32_t PlatformEventFilter;
enum PlatformEventFilter_ENUM : PlatformEventFilter
{
    PlatformEventFilter_MouseUp   = 1 << 0,
    PlatformEventFilter_MouseDown = 1 << 1,
    PlatformEventFilter_Mouse     = PlatformEventFilter_MouseUp|PlatformEventFilter_MouseDown,
    PlatformEventFilter_KeyUp     = 1 << 2,
    PlatformEventFilter_KeyDown   = 1 << 3,
    PlatformEventFilter_Keyboard  = PlatformEventFilter_KeyUp|PlatformEventFilter_KeyDown,
    PlatformEventFilter_Input     = PlatformEventFilter_Mouse|PlatformEventFilter_Keyboard,
    PlatformEventFilter_Text      = 1 << 4,
    PlatformEventFilter_Tick      = 1 << 5,
    PlatformEventFilter_ANY       = 0xFFFFFFFF,
};

function bool
MatchFilter(PlatformEventType type, PlatformEventFilter filter)
{
    bool result = !!(filter & (1 << (type - 1)));
    return result;
}

typedef int PlatformInputCode;
enum PlatformInputCode_ENUM : PlatformInputCode
{
    //
    // Key codes
    //

    PlatformInputCode_None           = 0x0,
    PlatformInputCode_LButton        = 0x1,
    PlatformInputCode_RButton        = 0x2,
    PlatformInputCode_Cancel         = 0x3,
    PlatformInputCode_MButton        = 0x4,
    PlatformInputCode_XButton1       = 0x5,
    PlatformInputCode_XButton2       = 0x6,
    PlatformInputCode_Back           = 0x8,
    PlatformInputCode_Tab            = 0x9,
    PlatformInputCode_Clear          = 0xC,
    PlatformInputCode_Return         = 0xD,
    PlatformInputCode_Shift          = 0x10,
    PlatformInputCode_Control        = 0x11,
    PlatformInputCode_Alt            = 0x12,
    PlatformInputCode_Pause          = 0x13,
    PlatformInputCode_CapsLock       = 0x14,
    PlatformInputCode_Kana           = 0x15,
    PlatformInputCode_Hangul         = 0x15,
    PlatformInputCode_Junja          = 0x17,
    PlatformInputCode_Final          = 0x18,
    PlatformInputCode_Hanja          = 0x19,
    PlatformInputCode_Kanji          = 0x19,
    PlatformInputCode_Escape         = 0x1B,
    PlatformInputCode_Convert        = 0x1C,
    PlatformInputCode_NonConvert     = 0x1D,
    PlatformInputCode_Accept         = 0x1E,
    PlatformInputCode_ModeChange     = 0x1F,
    PlatformInputCode_Space          = 0x20,
    PlatformInputCode_PageUp         = 0x21,
    PlatformInputCode_PageDown       = 0x22,
    PlatformInputCode_End            = 0x23,
    PlatformInputCode_Home           = 0x24,
    PlatformInputCode_Left           = 0x25,
    PlatformInputCode_Up             = 0x26,
    PlatformInputCode_Right          = 0x27,
    PlatformInputCode_Down           = 0x28,
    PlatformInputCode_Select         = 0x29,
    PlatformInputCode_Print          = 0x2A,
    PlatformInputCode_Execute        = 0x2B,
    PlatformInputCode_PrintScreen    = 0x2C,
    PlatformInputCode_Insert         = 0x2D,
    PlatformInputCode_Delete         = 0x2E,
    PlatformInputCode_Help           = 0x2F,
    PlatformInputCode_0              = '0',
    PlatformInputCode_1              = '1',
    PlatformInputCode_2              = '2',
    PlatformInputCode_3              = '3',
    PlatformInputCode_4              = '4',
    PlatformInputCode_5              = '5',
    PlatformInputCode_6              = '6',
    PlatformInputCode_7              = '7',
    PlatformInputCode_8              = '8',
    PlatformInputCode_9              = '9',
    /* 0x3A - 0x40: undefined */
    PlatformInputCode_A              = 'A',
    PlatformInputCode_B              = 'B',
    PlatformInputCode_C              = 'C',
    PlatformInputCode_D              = 'D',
    PlatformInputCode_E              = 'E',
    PlatformInputCode_F              = 'F',
    PlatformInputCode_G              = 'G',
    PlatformInputCode_H              = 'H',
    PlatformInputCode_I              = 'I',
    PlatformInputCode_J              = 'J',
    PlatformInputCode_K              = 'K',
    PlatformInputCode_L              = 'L',
    PlatformInputCode_M              = 'M',
    PlatformInputCode_N              = 'N',
    PlatformInputCode_O              = 'O',
    PlatformInputCode_P              = 'P',
    PlatformInputCode_Q              = 'Q',
    PlatformInputCode_R              = 'R',
    PlatformInputCode_S              = 'S',
    PlatformInputCode_T              = 'T',
    PlatformInputCode_U              = 'U',
    PlatformInputCode_V              = 'V',
    PlatformInputCode_W              = 'W',
    PlatformInputCode_X              = 'X',
    PlatformInputCode_Y              = 'Y',
    PlatformInputCode_Z              = 'Z',
    PlatformInputCode_LSys           = 0x5B,
    PlatformInputCode_RSys           = 0x5C,
    PlatformInputCode_Apps           = 0x5D,
    PlatformInputCode_Sleep          = 0x5f,
    PlatformInputCode_Numpad0        = 0x60,
    PlatformInputCode_Numpad1        = 0x61,
    PlatformInputCode_Numpad2        = 0x62,
    PlatformInputCode_Numpad3        = 0x63,
    PlatformInputCode_Numpad4        = 0x64,
    PlatformInputCode_Numpad5        = 0x65,
    PlatformInputCode_Numpad6        = 0x66,
    PlatformInputCode_Numpad7        = 0x67,
    PlatformInputCode_Numpad8        = 0x68,
    PlatformInputCode_Numpad9        = 0x69,
    PlatformInputCode_Multiply       = 0x6A,
    PlatformInputCode_Add            = 0x6B,
    PlatformInputCode_Separator      = 0x6C,
    PlatformInputCode_Subtract       = 0x6D,
    PlatformInputCode_Decimal        = 0x6E,
    PlatformInputCode_Divide         = 0x6f,
    PlatformInputCode_F1             = 0x70,
    PlatformInputCode_F2             = 0x71,
    PlatformInputCode_F3             = 0x72,
    PlatformInputCode_F4             = 0x73,
    PlatformInputCode_F5             = 0x74,
    PlatformInputCode_F6             = 0x75,
    PlatformInputCode_F7             = 0x76,
    PlatformInputCode_F8             = 0x77,
    PlatformInputCode_F9             = 0x78,
    PlatformInputCode_F10            = 0x79,
    PlatformInputCode_F11            = 0x7A,
    PlatformInputCode_F12            = 0x7B,
    PlatformInputCode_F13            = 0x7C,
    PlatformInputCode_F14            = 0x7D,
    PlatformInputCode_F15            = 0x7E,
    PlatformInputCode_F16            = 0x7F,
    PlatformInputCode_F17            = 0x80,
    PlatformInputCode_F18            = 0x81,
    PlatformInputCode_F19            = 0x82,
    PlatformInputCode_F20            = 0x83,
    PlatformInputCode_F21            = 0x84,
    PlatformInputCode_F22            = 0x85,
    PlatformInputCode_F23            = 0x86,
    PlatformInputCode_F24            = 0x87,
    PlatformInputCode_Numlock        = 0x90,
    PlatformInputCode_Scroll         = 0x91,
    PlatformInputCode_LShift         = 0xA0,
    PlatformInputCode_RShift         = 0xA1,
    PlatformInputCode_LControl       = 0xA2,
    PlatformInputCode_RControl       = 0xA3,
    PlatformInputCode_LAlt           = 0xA4,
    PlatformInputCode_RAlt           = 0xA5,
    /* 0xA6 - 0xAC: browser keys, not sure what's up with that */
    PlatformInputCode_VolumeMute     = 0xAD,
    PlatformInputCode_VolumeDown     = 0xAE,
    PlatformInputCode_VolumeUp       = 0xAF,
    PlatformInputCode_MediaNextTrack = 0xB0,
    PlatformInputCode_MediaPrevTrack = 0xB1,
    /* 0xB5 - 0xB7: "launch" keys, not sure what's up with that */
    PlatformInputCode_Oem1           = 0xBA, // misc characters, us standard: ';:'
    PlatformInputCode_Plus           = 0xBB,
    PlatformInputCode_Comma          = 0xBC,
    PlatformInputCode_Minus          = 0xBD,
    PlatformInputCode_Period         = 0xBE,
    PlatformInputCode_Oem2           = 0xBF, // misc characters, us standard: '/?'
    PlatformInputCode_Oem3           = 0xC0, // misc characters, us standard: '~'
    /* 0xC1 - 0xDA: reserved / unassigned */
    /* 0xDB - 0xF5: more miscellanious OEM codes I'm ommitting for now */
    /* 0xF6 - 0xF9: keys I've never heard of */
    PlatformInputCode_Play           = 0xFA,
    PlatformInputCode_Zoom           = 0xFB,
    PlatformInputCode_OemClear       = 0xFE,

    //
    //
    //

    PlatformInputCode_COUNT,
};

struct PlatformEvent
{
    PlatformEventType type;

    bool alt_down;
    bool ctrl_down;
    bool shift_down;

    bool pressed;
    bool repeat;
    PlatformInputCode input_code;

    int text_length;
    uint8_t *text;

    bool consumed_;
};

enum PlatformErrorType
{
    PlatformError_Fatal,
    PlatformError_Nonfatal,
};

enum PlatformMemFlag
{
    PlatformMemFlag_NoLeakCheck = 0x1,
};

struct PlatformHighResTime
{
    uint64_t opaque;
};

struct PlatformThreadHandle;
struct ThreadLocalContext;

struct PlatformJobQueue;
#define PLATFORM_JOB(name) void name(void *userdata)
typedef PLATFORM_JOB(PlatformJobProc);

#define PLATFORM_MAX_LOG_LINES 1024
#define PLATFORM_LOG_LINE_SIZE 1024

enum PlatformLogLevel
{
    PlatformLogLevel_Info    = 1,
    PlatformLogLevel_Warning = 2,
    PlatformLogLevel_Error   = 10,
};

struct PlatformLogLine
{
    PlatformLogLevel level;

    String string;
    uint8_t data_[PLATFORM_LOG_LINE_SIZE];
};

typedef uint32_t PlatformFontRasterFlags;
enum PlatformFontRasterFlags_ENUM : PlatformFontRasterFlags
{
    PlatformFontRasterFlag_RasterFont = 0x1,
    PlatformFontRasterFlag_DoNotMapUnicode = 0x2, // screw you, windows
};

struct PlatformEventIterator
{
    PlatformEventFilter filter;
    uint32_t index;
};

struct Platform
{
    bool exit_requested;

    bool app_initialized;
    bool exe_reloaded;
    void *persistent_app_data;

    String window_title;

    float dt;

    PlatformEventIterator (*IterateEvents)(PlatformEventFilter filter);
    bool (*NextEvent)(PlatformEventIterator *it, PlatformEvent *out_event);
    void (*PushTickEvent)(void);

    int32_t mouse_x, mouse_y, mouse_y_flipped;
    int32_t mouse_dx, mouse_dy;
    int32_t mouse_in_window;

    int32_t render_w, render_h;
    Bitmap backbuffer;

    void (*DebugPrint)(char *fmt, ...);
    void (*LogPrint)(PlatformLogLevel level, char *fmt, ...);
    void (*ReportError)(PlatformErrorType type, char *fmt, ...);

    PlatformLogLine *(*GetFirstLogLine)(void);
    PlatformLogLine *(*GetLatestLogLine)(void);
    PlatformLogLine *(*GetNextLogLine)(PlatformLogLine *);
    PlatformLogLine *(*GetPrevLogLine)(PlatformLogLine *);

    PlatformJobQueue *high_priority_queue;
    PlatformJobQueue *low_priority_queue;

    size_t page_size;
    size_t allocation_granularity;
    void *(*AllocateMemory)(size_t size, uint32_t flags, const char *tag);
    void *(*ReserveMemory)(size_t size, uint32_t flags, const char *tag);
    void *(*CommitMemory)(void *location, size_t size);
    void (*DecommitMemory)(void *location, size_t size);
    void (*DeallocateMemory)(void *memory);

    bool (*RegisterFontFile)(String file_name);
    bool (*MakeAsciiFont)(String font_name, Font *out_font, int font_size, PlatformFontRasterFlags flags);

    ThreadLocalContext *(*GetThreadLocalContext)(void);
    Arena *(*GetTempArena)(void);

    void (*AddJob)(PlatformJobQueue *queue, void *arg, PlatformJobProc *proc);
    void (*WaitForJobs)(PlatformJobQueue *queue);

    String (*ReadFile)(Arena *arena, String filename);
    size_t (*ReadFileInto)(size_t buffer_size, void *buffer, String filename);
    size_t (*GetFileSize)(String filename);

    PlatformHighResTime (*GetTime)(void);
    double (*SecondsElapsed)(PlatformHighResTime start, PlatformHighResTime end);

    bool (*WriteClipboard)(String text);
    String (*ReadClipboard)(Arena *arena);

    void (*SleepThread)(int milliseconds);
};

static Platform *platform;

function void
LeaveUnhandled(PlatformEvent *event)
{
    event->consumed_ = false;
}

#define APP_UPDATE_AND_RENDER(name) void name(Platform *platform)
typedef APP_UPDATE_AND_RENDER(AppUpdateAndRenderType);
extern "C" TEXTIT_EXPORT APP_UPDATE_AND_RENDER(AppUpdateAndRender);

#endif /* TEXTIT_PLATFORM_HPP */
