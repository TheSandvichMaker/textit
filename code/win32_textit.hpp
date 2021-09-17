#ifndef WIN32_TEXTIT_HPP
#define WIN32_TEXTIT_HPP

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <dwmapi.h>

#include "textit_platform.hpp"
#include "textit_shared.hpp"
#include "textit_memory.hpp"
#include "textit_string.hpp"
#include "textit_math.hpp"

struct Win32AllocationHeader
{
    Win32AllocationHeader *next, *prev;
    size_t size;
    char *base;
    uint32_t flags;
    const char *tag;
};

struct Win32AppCode
{
    bool valid;

    HMODULE dll;
    uint64_t last_write_time;

    AppUpdateAndRenderType *UpdateAndRender;
};

struct PlatformJobEntry
{
    PlatformJobProc *proc;
    void *params;
};

struct PlatformJobQueue
{
    HANDLE stop;
    HANDLE done;
    HANDLE run;

    int thread_count;
    HANDLE *threads;
    ThreadLocalContext *tls;

    volatile uint32_t jobs_in_flight;
    volatile uint32_t next_write;
    volatile uint32_t next_read;
    PlatformJobEntry jobs[256];

    StaticAssert(IsPow2(ArrayCount(jobs)), "Jobs array must be a power of 2");
};

struct ThreadLocalContext
{
    ThreadLocalContext *next;

    Arena *temp_arena;
    Arena *prev_temp_arena;

    Arena temp_arena_1_;
    Arena temp_arena_2_;

    HANDLE input_wait_event;
};

struct Win32State
{
    Arena arena;
    Arena temp_arena;

    bool window_class_registered;
    WNDCLASSW window_class;
    HWND window;
    POINT window_to_client_tl_delta;
    POINT window_to_client_br_delta;

    HRGN current_text_clip_rect;

    bool late_latching;
    uint64_t update_time_sample_count;
    double   update_time_accumulator;

    wchar_t *exe_folder;
    wchar_t *dll_path;
    Win32AppCode app_code;

    DWORD thread_local_index;

    TicketMutex allocation_mutex;
    Win32AllocationHeader allocation_sentinel;

    TicketMutex log_mutex;
    int log_line_count;
    int log_line_first;
    PlatformLogLine log_lines[PLATFORM_MAX_LOG_LINES];

    uint32_t event_read_index;
    uint32_t event_write_index;
    uint32_t working_event_write_index;
    PlatformEvent null_event;
    PlatformEvent events[4096];
};

#endif /* WIN32_TEXTIT_HPP */
