#include "win32_textit.hpp"

//
// TODO: Run game on separate thread from the windows guff
// Presumably this requires no longer relying on Dwm for vsync which is fine
// Instead of OpenGL I could do this: https://gist.github.com/mmozeiko/6f36f2b82204b70a9b7fe6c05ccd868f?ts=4
// Although for just showing textures a jank immediate mode OpenGL context would work fine
//

static bool g_running;
static LARGE_INTEGER g_perf_freq;
static WINDOWPLACEMENT g_window_position = { sizeof(g_window_position) };

static Win32State win32_state;
static Platform platform_;

extern "C"
{
__declspec(dllexport) unsigned long NvOptimusEnablement        = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static inline bool
Win32_NextEvent(PlatformEvent **out_event, PlatformEventFilter filter)
{
    bool result = false;

    PlatformEvent *it = *out_event;
    if (!it)
    {
        it = platform->first_event;
    }
    else
    {
        it = it->next;
    }

    while (it)
    {
        if (!it->consumed_ && MatchFilter(it->type, filter))
        {
            result = true;
            it->consumed_ = true;
            *out_event = it;
            break;
        }
        it = it->next;
    }

    return result;
}

static void *
Win32_Reserve(size_t size, uint32_t flags, const char *tag)
{
    size_t page_size = platform->page_size;
    size_t total_size = page_size + size;

    Win32AllocationHeader *header = (Win32AllocationHeader *)VirtualAlloc(0, total_size, MEM_RESERVE, PAGE_NOACCESS);
    VirtualAlloc(header, page_size, MEM_COMMIT, PAGE_READWRITE);

    header->size = total_size;
    header->base = (char *)header + page_size;
    header->flags = flags;
    header->tag = tag;

    header->next = &win32_state.allocation_sentinel;
    header->prev = win32_state.allocation_sentinel.prev;
    header->next->prev = header;
    header->prev->next = header;

    return header->base;
}

static void *
Win32_Commit(void *Pointer, size_t Size)
{
    void *Result = VirtualAlloc(Pointer, Size, MEM_COMMIT, PAGE_READWRITE);
    return Result;
}

static void *
Win32_Allocate(size_t Size, uint32_t Flags, const char *Tag)
{
    void *Result = Win32_Reserve(Size, Flags, Tag);
    if (Result)
    {
        Result = Win32_Commit(Result, Size);
    }
    return Result;
}

static void
Win32_Decommit(void *pointer, size_t size)
{
    if (pointer)
    {
        VirtualFree(pointer, size, MEM_DECOMMIT);
    }
}

static void
Win32_Deallocate(void *pointer)
{
    if (pointer)
    {
        Win32AllocationHeader *header = (Win32AllocationHeader *)((char *)pointer - platform->page_size);
        header->prev->next = header->next;
        header->next->prev = header->prev;
        VirtualFree(header, 0, MEM_RELEASE);
    }
}

static inline wchar_t *
Win32_FormatLastError(void)
{
    wchar_t *message = NULL;
    DWORD error = GetLastError() & 0xFFFF;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER| 
                   FORMAT_MESSAGE_FROM_SYSTEM|
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   error,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (wchar_t *)&message,
                   0, NULL);

    return message;
}

static inline void
Win32_DisplayLastError(void)
{
    wchar_t *message = Win32_FormatLastError();
    MessageBoxW(0, message, L"Error", MB_OK);
    LocalFree(message);
}

static inline void
Win32_ExitWithLastError(int exit_code = -1)
{
    Win32_DisplayLastError();
    ExitProcess(exit_code);
}

static inline wchar_t *
Win32_Utf8ToUtf16(Arena *arena, const char *utf8, int length = -1)
{
    wchar_t *result = nullptr;

    int wchar_count = MultiByteToWideChar(CP_UTF8, 0, utf8, length, nullptr, 0);
    int null_terminated_count = (length == -1 ? wchar_count : wchar_count + 1);

    result = PushArrayNoClear(arena, null_terminated_count, wchar_t);

    if (result)
    {
        if (!MultiByteToWideChar(CP_UTF8, 0, utf8, length, result, wchar_count))
        {
            Win32_DisplayLastError();
        }

        result[null_terminated_count - 1] = 0;
    }

    return result;
}

static inline uint8_t *
Win32_Utf16ToUtf8(Arena *arena, const wchar_t *utf16, int *out_length = nullptr)
{
    uint8_t *result = nullptr;

    int char_count = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, nullptr, nullptr);
    result = PushArrayNoClear(arena, char_count, uint8_t);

    if (result)
    {
        if (!WideCharToMultiByte(CP_UTF8, 0, utf16, -1, (LPSTR)result, char_count, nullptr, nullptr))
        {
            Win32_DisplayLastError();
        }

        if (out_length) *out_length = char_count - 1;
    }
    else
    {
        if (out_length) *out_length = 0;
    }

    return result;
}

static inline wchar_t *
FormatWStringV(Arena *arena, wchar_t *fmt, va_list args_init)
{
    va_list args_size;
    va_copy(args_size, args_init);

    va_list args_fmt;
    va_copy(args_fmt, args_init);

    int chars_required = _vsnwprintf(nullptr, 0, fmt, args_size) + 1;
    va_end(args_size);

    wchar_t *result = PushArrayNoClear(arena, chars_required, wchar_t);
    _vsnwprintf(result, chars_required, fmt, args_fmt);
    va_end(args_fmt);

    return result;
}

static inline wchar_t *
FormatWString(Arena *arena, wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    wchar_t *result = FormatWStringV(arena, fmt, args);
    va_end(args);
    return result;
}

static inline char *
FormatStringV(Arena *arena, char *fmt, va_list args_init)
{
    va_list args_size;
    va_copy(args_size, args_init);

    va_list args_fmt;
    va_copy(args_fmt, args_init);

    int chars_required = vsnprintf(nullptr, 0, fmt, args_size) + 1;
    va_end(args_size);

    char *result = PushArrayNoClear(arena, chars_required, char);
    vsnprintf(result, chars_required, fmt, args_fmt);
    va_end(args_fmt);

    return result;
}

static inline char *
FormatString(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *result = FormatStringV(arena, fmt, args);
    va_end(args);
    return result;
}

static inline void
Win32_DebugPrint(char *fmt, ...)
{
    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, fmt);
    char *formatted = FormatStringV(arena, fmt, args);
    va_end(args);

    wchar_t *fmt_wide = Win32_Utf8ToUtf16(arena, formatted);

    OutputDebugStringW(fmt_wide);
}

static inline void
Win32_LogPrint(PlatformLogLevel level, char *fmt, ...)
{
    Win32State *state = &win32_state;

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, fmt);
    char *formatted = FormatStringV(arena, fmt, args);
    va_end(args);

    char *at = formatted;
    while (*at)
    {
        char *line_start = at;
        while (*at && at[0] != '\n' && !(at[0] == '\r' && at[1] == '\n'))
        {
            at += 1;
        }
        size_t line_length = at - line_start + 1;

        if (line_length > PLATFORM_LOG_LINE_SIZE)
        {
            line_length = PLATFORM_LOG_LINE_SIZE;
        }

        BeginTicketMutex(&state->log_mutex);

        PlatformLogLine *line = &state->log_lines[(state->log_line_first + state->log_line_count) % PLATFORM_MAX_LOG_LINES];
        if (state->log_line_count < PLATFORM_MAX_LOG_LINES)
        {
            state->log_line_count += 1;
        }
        else
        {
            state->log_line_first += 1;
        }

        EndTicketMutex(&state->log_mutex);

        CopySize(line_length, line_start, line->data_);
        line->data_[line_length] = 0;

        line->string.size = line_length;
        line->string.data = line->data_;

        line->level = level;

        while (*at && (at[0] == '\n' || (at[0] == '\r' && at[1] == '\n')))
        {
            at += 1;
        }
    }
}

static inline PlatformLogLine *
Win32_GetFirstLogLine(void)
{
    PlatformLogLine *result = &win32_state.log_lines[win32_state.log_line_first];
    return result;
}

static inline PlatformLogLine *
Win32_GetLatestLogLine(void)
{
    PlatformLogLine *result = &win32_state.log_lines[(win32_state.log_line_first + win32_state.log_line_count - 1) % PLATFORM_MAX_LOG_LINES];
    return result;
}

static inline PlatformLogLine *
Win32_GetNextLogLine(PlatformLogLine *line)
{
    Assert((line >= win32_state.log_lines) &&
           (line < (win32_state.log_lines + PLATFORM_MAX_LOG_LINES)));

    PlatformLogLine *next = line + 1;
    if (next >= (win32_state.log_lines + PLATFORM_MAX_LOG_LINES))
    {
        next -= PLATFORM_MAX_LOG_LINES;
    }

    if (next == &win32_state.log_lines[(win32_state.log_line_first + win32_state.log_line_count) % PLATFORM_MAX_LOG_LINES])
    {
        return nullptr;
    }

    return next;
}

static inline PlatformLogLine *
Win32_GetPrevLogLine(PlatformLogLine *line)
{
    Assert((line >= win32_state.log_lines) &&
           (line < (win32_state.log_lines + PLATFORM_MAX_LOG_LINES)));

    if (line == &win32_state.log_lines[win32_state.log_line_first])
    {
        return nullptr;
    }

    PlatformLogLine *prev = line - 1;
    if (prev < win32_state.log_lines)
    {
        prev += PLATFORM_MAX_LOG_LINES;
    }

    return prev;
}

static inline void
Win32_ReportError(PlatformErrorType type, char *error, ...)
{
    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, error);
    char *formatted_error = FormatStringV(arena, error, args);
    va_end(args);

    wchar_t *error_wide = Win32_Utf8ToUtf16(arena, formatted_error);
    if (error_wide)
    {
        MessageBoxW(0, error_wide, L"Error", MB_OK);
    }

    if (type == PlatformError_Fatal)
    {
#if TEXTIT_INTERNAL
        __debugbreak();
#else
        ExitProcess(1);
#endif
    }
}

static size_t
Win32_ReadFileInto(size_t buffer_size, void *buffer, String filename)
{
    Assert((buffer_size == 0) || buffer);

    size_t result = 0;

    ScopedMemory filename_temp_memory(&win32_state.temp_arena);
    wchar_t *file_wide = Win32_Utf8ToUtf16(&win32_state.temp_arena, (char *)filename.data, (int)filename.size);

    HANDLE handle = CreateFileW(file_wide, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        DWORD file_size_high;
        DWORD file_size_low = GetFileSize(handle, &file_size_high);
        // NOTE: Right now we're just doing 32 bit file IO.
        if (!file_size_high) {
            DWORD bytes_to_read = file_size_low;
            if (bytes_to_read > buffer_size - 1)
            {
                bytes_to_read = (DWORD)(buffer_size - 1);
            }

            DWORD bytes_read;
            if (ReadFile(handle, buffer, file_size_low, &bytes_read, 0))
            {
                if (bytes_read == bytes_to_read)
                {
                    result = bytes_to_read;
                    ((char *)buffer)[bytes_to_read] = 0; // null terminate, just for convenience.
                }
                else
                {
                    Win32_DebugPrint("Did not read expected number of bytes from file '%s'\n", filename);
                }
            }
            else
            {
                Win32_DebugPrint("Could not read file '%s'\n", filename);
            }
        }
    }
    else
    {
        Win32_DebugPrint("Could not open file '%s'\n", filename);
    }

    return result;
}

static String
Win32_ReadFile(Arena *arena, String filename)
{
    String result = {};

    ScopedMemory filename_temp_memory(&win32_state.temp_arena);
    wchar_t *file_wide = Win32_Utf8ToUtf16(&win32_state.temp_arena, (char *)filename.data, (int)filename.size);

    HANDLE handle = CreateFileW(file_wide, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        DWORD file_size_high;
        DWORD file_size_low = GetFileSize(handle, &file_size_high);
        // NOTE: Right now we're just doing 32 bit file IO.
        if (!file_size_high) {
            ScopedMemory temp_memory(arena);

            result.data = PushArrayNoClear(arena, file_size_low + 1, uint8_t);

            DWORD bytes_read;
            if (ReadFile(handle, result.data, file_size_low, &bytes_read, 0))
            {
                if (bytes_read == file_size_low)
                {
                    result.size = file_size_low;
                    result.data[file_size_low] = 0; // null terminate, just for convenience.
                    CommitTemporaryMemory(temp_memory);
                }
                else
                {
                    Win32_DebugPrint("Did not read expected number of bytes from file '%s'\n", filename);
                }
            }
            else
            {
                Win32_DebugPrint("Could not read file '%s'\n", filename);
            }
        }
    }
    else
    {
        Win32_DebugPrint("Could not open file '%s'\n", filename);
    }

    return result;
}

static inline void
Win32_ToggleFullscreen(HWND window)
{
    // Raymond Chen's fullscreen toggle recipe:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(window, &g_window_position) &&
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi))
        {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_window_position);
        SetWindowPos(window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

static inline void
Win32_ResizeOffscreenBuffer(Bitmap *buffer, int32_t w, int32_t h)
{
    if (buffer->data &&
        w != buffer->w &&
        h != buffer->h)
    {
        Win32_Deallocate(buffer->data);
        ZeroStruct(buffer);
    }

    if (!buffer->data &&
        w > 0 &&
        h > 0)
    {
        buffer->data = (Color *)Win32_Allocate(Align16(sizeof(Color)*w*h), 0, LOCATION_STRING("Win32OffscreenBuffer"));
        buffer->w = w;
        buffer->h = h;
        buffer->pitch = w;
    }
}

static inline void
Win32_DisplayOffscreenBuffer(HWND window, Bitmap *buffer)
{
    HDC dc = GetDC(window);

    RECT client_rect;
    GetClientRect(window, &client_rect);

    int dst_w = client_rect.right;
    int dst_h = client_rect.bottom;

    BITMAPINFO bitmap_info = {};
    BITMAPINFOHEADER *bitmap_header = &bitmap_info.bmiHeader;
    bitmap_header->biSize = sizeof(*bitmap_header);
    bitmap_header->biWidth = buffer->w;
    bitmap_header->biHeight = buffer->h;
    bitmap_header->biPlanes = 1;
    bitmap_header->biBitCount = 32;
    bitmap_header->biCompression = BI_RGB;

    StretchDIBits(dc,
                  0, 0, dst_w, dst_h,
                  0, 0, buffer->w, buffer->h,
                  buffer->data,
                  &bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    ReleaseDC(window, dc);
}

static LRESULT CALLBACK 
Win32_WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            g_running = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            BeginPaint(window, &paint);

            RECT client_rect;
            GetClientRect(window, &client_rect);

            platform->render_w = client_rect.right;
            platform->render_h = client_rect.bottom;

            Win32_ResizeOffscreenBuffer(&platform->backbuffer, platform->render_w, platform->render_h);
            Win32_DisplayOffscreenBuffer(window, &platform->backbuffer);

            EndPaint(window, &paint);
        } break;
        
        default:
        {
            result = DefWindowProcW(window, message, w_param, l_param);
        } break;
    }
    return result;
}

static HWND
Win32_CreateWindow(HINSTANCE instance, int x, int y, int w, int h, const wchar_t *title)
{
    int window_x = x;
    int window_y = y;
    int render_w = w;
    int render_h = h;

    RECT window_rect = { window_x, window_y, window_x + render_w, window_y + render_h };
    AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, false, 0);

    int window_w = window_rect.right - window_rect.left;
    int window_h = window_rect.bottom - window_rect.top;

    WNDCLASSW window_class = {};
    window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = Win32_WindowProc;
    window_class.hInstance = instance;
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = L"Win32WindowClass";

    if (!RegisterClassW(&window_class))
    {
        Win32_ExitWithLastError();
    }

    HWND window_handle = CreateWindowW(window_class.lpszClassName,
                                       title,
                                       WS_OVERLAPPEDWINDOW,
                                       window_x, window_y,
                                       window_w, window_h,
                                       NULL, NULL, instance, NULL);

    if (!window_handle)
    {
        Win32_ExitWithLastError();
    }

    return window_handle;
}

static inline PlatformEvent *
PushEvent()
{
    PlatformEvent *result = PushStruct(&win32_state.temp_arena, PlatformEvent);
    if (platform->first_event)
    {
        platform->last_event = platform->last_event->next = result;
    }
    else
    {
        platform->first_event = platform->last_event = result;
    }
    return result;
}

static void
Win32_PushTickEvent(void)
{
    PlatformEvent *event = PushEvent();
    event->type = PlatformEvent_Tick;
}

static bool
Win32_HandleSpecialKeys(HWND window, int vk_code, bool pressed, bool alt_is_down)
{
    bool processed = false;

    if (!pressed)
    {
        if (alt_is_down)
        {
            processed = true;
            switch (vk_code)
            {
                case VK_RETURN:
                {
                    Win32_ToggleFullscreen(window);
                } break;

                case VK_F4:
                {
                    g_running = false;
                } break;

                default:
                {
                    processed = false;
                } break;
            }
        }
    }

    return processed;
}

static PlatformHighResTime
Win32_GetTime(void)
{
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);

    PlatformHighResTime result;
    result.opaque = (uint64_t &)time.QuadPart;

    return result;
}

static double
Win32_SecondsElapsed(PlatformHighResTime start, PlatformHighResTime end)
{
    double result = (double)(end.opaque - start.opaque) / (double)g_perf_freq.QuadPart;
    return result;
}

static wchar_t *
FindExeFolderLikeAMonkeyInAMonkeySuit(void)
{
    DWORD exe_path_count = 0;
    wchar_t *exe_path = nullptr;
    size_t exe_buffer_count = MAX_PATH;
    for (;;)
    {
        ScopedMemory temp(&win32_state.arena);
        exe_path = PushArrayNoClear(&win32_state.arena, exe_buffer_count, wchar_t);

        SetLastError(0);
        exe_path_count = GetModuleFileNameW(0, exe_path, (DWORD)exe_buffer_count);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            CommitTemporaryMemory(temp);
            break;
        }

        exe_buffer_count *= 2;
    }

    wchar_t *last_backslash = exe_path;
    for (wchar_t *c = exe_path; *c; ++c)
    {
        if (*c == L'\\')
        {
            last_backslash = c;
        }
    }

    *last_backslash = 0;

    return exe_path;
}

static inline bool
Win32_FileExists(wchar_t *path)
{
    bool result = false;

    WIN32_FILE_ATTRIBUTE_DATA gibberish;
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &gibberish))
    {
        result = true;
    }

    return result;
}

static inline uint64_t
Win32_GetLastWriteTime(wchar_t *path)
{
    FILETIME last_write_time = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &data))
    {
        last_write_time = data.ftLastWriteTime;
    }

     ULARGE_INTEGER thanks;
     thanks.LowPart  = last_write_time.dwLowDateTime;
     thanks.HighPart = last_write_time.dwLowDateTime;

    return thanks.QuadPart;
}

static int dll_generation;
static bool
Win32_LoadAppCode(Win32AppCode *old_code)
{
    bool result = false;

    ScopedMemory temp(&win32_state.arena);
    wchar_t *exe_folder         = win32_state.exe_folder;
    wchar_t *dll_path           = win32_state.dll_path;
    wchar_t *lock_file_path     = FormatWString(&win32_state.arena, L"\\\\?\\%s\\textit_lock.temp", exe_folder);
    wchar_t *prev_temp_dll_path = FormatWString(&win32_state.arena, L"\\\\?\\%s\\temp_textit_%d.dll", exe_folder, dll_generation);
    wchar_t *temp_dll_path      = FormatWString(&win32_state.arena, L"\\\\?\\%s\\temp_textit_%d.dll", exe_folder, dll_generation + 1);

    Win32AppCode new_code = {};

    if (!Win32_FileExists(lock_file_path))
    {
        DeleteFileW(prev_temp_dll_path);
        CopyFileW(dll_path, temp_dll_path, false);

        dll_generation += 1;

        new_code.dll = LoadLibraryW(temp_dll_path);
        if (new_code.dll)
        {
            new_code.last_write_time = Win32_GetLastWriteTime(dll_path);
            new_code.UpdateAndRender = (AppUpdateAndRenderType *)GetProcAddress(new_code.dll, "AppUpdateAndRender");

            new_code.valid = true;
            if (!new_code.UpdateAndRender)
            {
                new_code.valid = false;
                Win32_DebugPrint("Could not load AppUpdateAndRender from app dll\n");
            }

            if (new_code.valid)
            {
                result = true;
                *old_code = new_code;
            }
        }
    }

    return result;
}

static inline void
Win32_InitializeTLSForThread(ThreadLocalContext *context)
{
    if (!TlsSetValue(win32_state.thread_local_index, context))
    {
        Win32_ExitWithLastError();
    }

    context->temp_arena      = &context->temp_arena_1_;
    context->prev_temp_arena = &context->temp_arena_2_;

    SetCapacity(context->temp_arena, Megabytes(4));
    SetCapacity(context->prev_temp_arena, Megabytes(4));
}

static ThreadLocalContext *
Win32_GetThreadLocalContext(void)
{
    ThreadLocalContext *result = (ThreadLocalContext *)TlsGetValue(win32_state.thread_local_index);
    if (!result)
    {
        Win32_ExitWithLastError();
    }
    return result;
}

static void
Win32_DestroyThreadLocalContext(void)
{
    ThreadLocalContext *context = Win32_GetThreadLocalContext();
    if (context)
    {
        Release(context->temp_arena);
        Release(context->prev_temp_arena);
    }
    else
    {
        INVALID_CODE_PATH;
    }
}

static inline Arena *
Win32_GetTempArena(void)
{
    ThreadLocalContext *context = Win32_GetThreadLocalContext();
    Arena *result = context->temp_arena;
    return result;
}

struct Win32JobThreadParams
{
    ThreadLocalContext *context;
    PlatformJobQueue *queue;
    HANDLE ready;
};

static DWORD WINAPI
Win32_JobThreadProc(LPVOID userdata)
{
    Win32JobThreadParams *params = (Win32JobThreadParams *)userdata;

    Win32_InitializeTLSForThread(params->context);
    PlatformJobQueue *queue = params->queue;
    SetEvent(params->ready);

    for (;;)
    {
        uint32_t entry_index = queue->next_read;
        if (entry_index != queue->next_write)
        {
            uint32_t next_entry_index = entry_index + 1;
            uint32_t exchanged_index = InterlockedCompareExchange(&queue->next_read, next_entry_index, entry_index);

            if (entry_index == exchanged_index)
            {
                // TODO: Double buffered temp arenas are less useful in this context, not sure what the right design is,
                // not that important for now though.
                ThreadLocalContext *context = platform->GetThreadLocalContext();
                Swap(context->temp_arena, context->prev_temp_arena);
                Clear(context->temp_arena);

                PlatformJobEntry *job = &queue->jobs[entry_index % ArrayCount(queue->jobs)];
                job->proc(job->params);

                uint32_t jobs_count = InterlockedDecrement(&queue->jobs_in_flight);
                if (jobs_count == 0)
                {
                    SetEvent(queue->done);
                }
            }
        }
        else
        {
            HANDLE handles[] = { queue->stop, queue->run };
            if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
            {
                break;
            }
        }
    }

    Win32_DestroyThreadLocalContext();

    return 0;
}

static void
Win32_InitializeJobQueue(PlatformJobQueue *queue, int thread_count)
{
    queue->stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    queue->done = CreateEventA(NULL, TRUE, FALSE, NULL);
    queue->run = CreateSemaphoreA(NULL, 0, thread_count, NULL);

    queue->tls = PushArray(&win32_state.arena, thread_count, ThreadLocalContext);

    queue->thread_count = thread_count;
    queue->threads = PushArray(&win32_state.arena, thread_count, HANDLE);

    HANDLE ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    for (int i = 0; i < thread_count; ++i)
    {
        Win32JobThreadParams params = {};
        params.context = &queue->tls[i];
        params.queue = queue;
        params.ready = ready;

        queue->threads[i] = CreateThread(NULL, 0, Win32_JobThreadProc, &params, 0, NULL);
        WaitForSingleObject(ready, INFINITE);
    }
    CloseHandle(ready);
}

static void
Win32_SleepThread(int milliseconds)
{
    Sleep(milliseconds);
}

static void
Win32_AddJob(PlatformJobQueue *queue, void *params, PlatformJobProc *proc)
{
    uint32_t new_next_write = queue->next_write + 1;

    PlatformJobEntry *entry = &queue->jobs[queue->next_write % ArrayCount(queue->jobs)];
    entry->proc = proc;
    entry->params = params;

    MemoryBarrier();

    queue->next_write = new_next_write;

    InterlockedIncrement(&queue->jobs_in_flight);
    ResetEvent(queue->done);

    ReleaseSemaphore(queue->run, 1, NULL);
}

static void
Win32_WaitForJobs(PlatformJobQueue *queue)
{
    WaitForSingleObject(queue->done, INFINITE);
}

static void
Win32_CloseJobQueue(PlatformJobQueue *queue)
{
    SetEvent(queue->stop);
    for (int i = 0; i < queue->thread_count; ++i)
    {
        WaitForSingleObject(queue->threads[i], INFINITE);
        CloseHandle(queue->threads[i]);
    }
    CloseHandle(queue->stop);
    CloseHandle(queue->run);
    CloseHandle(queue->done);
}

int
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_cmd)
{
    UNUSED_VARIABLE(command_line);
    UNUSED_VARIABLE(prev_instance);

    platform = &platform_;

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    QueryPerformanceFrequency(&g_perf_freq);

    win32_state.allocation_sentinel.next = &win32_state.allocation_sentinel;
    win32_state.allocation_sentinel.prev = &win32_state.allocation_sentinel;

    PlatformJobQueue high_priority_queue = {};
    PlatformJobQueue  low_priority_queue = {};

    platform->page_size = system_info.dwPageSize;
    platform->NextEvent = Win32_NextEvent;
    platform->PushTickEvent = Win32_PushTickEvent;
    platform->high_priority_queue = &high_priority_queue;
    platform->low_priority_queue = &low_priority_queue;
    platform->DebugPrint = Win32_DebugPrint;
    platform->LogPrint = Win32_LogPrint;
    platform->GetFirstLogLine = Win32_GetFirstLogLine;
    platform->GetLatestLogLine = Win32_GetLatestLogLine;
    platform->GetNextLogLine = Win32_GetNextLogLine;
    platform->GetPrevLogLine = Win32_GetPrevLogLine;
    platform->ReportError = Win32_ReportError;
    platform->AllocateMemory = Win32_Allocate;
    platform->ReserveMemory = Win32_Reserve;
    platform->CommitMemory = Win32_Commit;
    platform->DecommitMemory = Win32_Decommit;
    platform->DeallocateMemory = Win32_Deallocate;
    platform->GetThreadLocalContext = Win32_GetThreadLocalContext;
    platform->GetTempArena = Win32_GetTempArena;
    platform->AddJob = Win32_AddJob;
    platform->WaitForJobs = Win32_WaitForJobs;
    platform->ReadFile = Win32_ReadFile;
    platform->ReadFileInto = Win32_ReadFileInto;
    platform->GetTime = Win32_GetTime;
    platform->SecondsElapsed = Win32_SecondsElapsed;
    platform->SleepThread = Win32_SleepThread;

    win32_state.thread_local_index = TlsAlloc();
    if (win32_state.thread_local_index == TLS_OUT_OF_INDEXES)
    {
        Win32_ExitWithLastError();
    }

    ThreadLocalContext tls_context = {};
    Win32_InitializeTLSForThread(&tls_context);

    Win32_InitializeJobQueue(&high_priority_queue, 8);
    Win32_InitializeJobQueue(&low_priority_queue, 4);

    win32_state.exe_folder = FindExeFolderLikeAMonkeyInAMonkeySuit();
    win32_state.dll_path   = FormatWString(&win32_state.arena, L"\\\\?\\%s\\textit.dll", win32_state.exe_folder);

    Win32AppCode *app_code = &win32_state.app_code;
    if (!Win32_LoadAppCode(app_code))
    {
        platform->ReportError(PlatformError_Fatal, "Could not load app code");
    }
     platform->exe_reloaded = true;

    HCURSOR arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);
    HWND window = Win32_CreateWindow(instance, 32, 32, 720, 480, L"Dungeons");
    if (!window)
    {
        platform->ReportError(PlatformError_Fatal, "Could not create window");
    }

    ShowWindow(window, show_cmd);

    BOOL composition_enabled;
    if (DwmIsCompositionEnabled(&composition_enabled) != S_OK)
    {
        Win32_DisplayLastError();
    }

    double smooth_frametime = 1.0f / 60.0f;
    PlatformHighResTime frame_start_time = Win32_GetTime();

    platform->dt = 1.0f / 60.0f;

    g_running = true;
    while (g_running)
    {
        Clear(&win32_state.temp_arena);

        {
            ThreadLocalContext *context = &tls_context;
            Swap(context->temp_arena, context->prev_temp_arena);
            Clear(context->temp_arena);
        }

        bool exit_requested = false;
        platform->first_event = platform->last_event = nullptr;

        MSG message;
        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            switch (message.message)
            {
                case WM_CLOSE:
                case WM_QUIT:
                {
                    exit_requested = true;
                } break;

                case WM_KEYUP:
                case WM_KEYDOWN:
                case WM_SYSKEYUP:
                case WM_SYSKEYDOWN:
                {
					int vk_code = (int)message.wParam;
					bool pressed = (message.lParam & (1ull << 31)) == 0;
                    bool alt_down = (message.lParam & (1 << 29)) != 0;
                    bool ctrl_down = !!(GetKeyState(VK_CONTROL) & 0xFF00);
                    bool shift_down = !!(GetKeyState(VK_SHIFT) & 0xFF00);

                    if (!Win32_HandleSpecialKeys(window, vk_code, pressed, alt_down))
                    {
                        PlatformEvent *event = PushEvent();
                        event->type = (pressed ? PlatformEvent_KeyDown : PlatformEvent_KeyUp);
                        event->pressed = pressed;
                        event->alt_down = alt_down;
                        event->ctrl_down = ctrl_down;
                        event->shift_down = shift_down;
                        event->input_code = (PlatformInputCode)vk_code; // I gave myself a 1:1 mapping of VK codes to platform input codes, so that's nice.

                        TranslateMessage(&message);
                    }
                } break;

                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                {
                    PlatformEvent *event = PushEvent();
                    event->pressed = (message.message == WM_LBUTTONDOWN ||
                                      message.message == WM_MBUTTONDOWN ||
                                      message.message == WM_RBUTTONDOWN ||
                                      message.message == WM_XBUTTONDOWN);
                    event->type = (event->pressed ? PlatformEvent_MouseDown : PlatformEvent_MouseUp);
                    switch (message.message)
                    {
                        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                        {
                            event->input_code = PlatformInputCode_LButton;
                        } break;
                        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                        {
                            event->input_code = PlatformInputCode_MButton;
                        } break;
                        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                        {
                            event->input_code = PlatformInputCode_RButton;
                        } break;
                        case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                        {
                            if (GET_XBUTTON_WPARAM(message.wParam) == XBUTTON1)
                            {
                                event->input_code = PlatformInputCode_XButton1;
                            }
                            else
                            {
                                Assert(GET_XBUTTON_WPARAM(message.wParam) == XBUTTON2);
                                event->input_code = PlatformInputCode_XButton2;
                            }
                        } break;
                    }
                } break;

                case WM_CHAR:
                {
                    PlatformEvent *event = PushEvent();
                    wchar_t buf[] = { (wchar_t)message.wParam, 0 };
                    if (buf[0] == L'\r')
                    {
                        buf[0] = L'\n';
                    }
                    event->type = PlatformEvent_Text;
                    event->text = Win32_Utf16ToUtf8(&win32_state.temp_arena, buf, &event->text_length);
                } break;

                default:
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                } break;
            }
        }

        RECT client_rect;
        GetClientRect(window, &client_rect);

        int32_t client_w = client_rect.right;
        int32_t client_h = client_rect.bottom;

        platform->render_w = client_w;
        platform->render_h = client_h;

        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        ScreenToClient(window, &cursor_pos);

        platform->mouse_x = cursor_pos.x;
        platform->mouse_y = client_h - cursor_pos.y - 1;
        platform->mouse_y_flipped = cursor_pos.y;
        platform->mouse_in_window = (platform->mouse_x >= 0 && platform->mouse_x < client_w &&
                                     platform->mouse_y >= 0 && platform->mouse_y < client_h);

        if (platform->mouse_in_window)
        {
            SetCursor(arrow_cursor);
        }

        Win32_ResizeOffscreenBuffer(&platform->backbuffer,
                                    platform->render_w,
                                    platform->render_h);

        Bitmap *buffer = &platform->backbuffer;

        if (app_code->valid)
        {
            app_code->UpdateAndRender(platform);
            platform->exe_reloaded = false;
        }
        Win32_DisplayOffscreenBuffer(window, buffer);

        if (composition_enabled)
        {
            DwmFlush();
        }

        PlatformHighResTime frame_end_time = Win32_GetTime();
        double seconds_elapsed = Win32_SecondsElapsed(frame_start_time, frame_end_time);
        Swap(frame_start_time, frame_end_time);

        smooth_frametime = 0.9f*smooth_frametime + 0.1f*seconds_elapsed;
        wchar_t *title = FormatWString(&win32_state.temp_arena, L"Dungeons - frame time: %fms, fps: %f\n",
                                       1000.0*smooth_frametime,
                                       1.0 / smooth_frametime);
        SetWindowTextW(window, title);

        platform->dt = (float)seconds_elapsed;
        if (platform->dt > 1.0f / 15.0f)
        {
            platform->dt = 1.0f / 15.0f;
        }

        uint64_t last_write_time = Win32_GetLastWriteTime(win32_state.dll_path);
        if (app_code->last_write_time != last_write_time)
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                if (Win32_LoadAppCode(app_code))
                {
                    platform->exe_reloaded = true;
                    break;
                }

                Sleep(100);
            }
        }

        if (exit_requested)
        {
            g_running = false;
        }
    }

    Win32_CloseJobQueue(platform->high_priority_queue);
    Win32_CloseJobQueue(platform->low_priority_queue);
    Win32_ResizeOffscreenBuffer(&platform->backbuffer, 0, 0);

    bool leaked_memory = false;
    for (Win32AllocationHeader *header = win32_state.allocation_sentinel.next;
         header != &win32_state.allocation_sentinel;
         header = header->next)
    {
        Win32_DebugPrint("Allocated Block, Size: %llu, Tag: %s, NoLeakCheck: %s\n",
                         header->size,
                         header->tag,
                         (header->flags & PlatformMemFlag_NoLeakCheck ? "true" : "false"));
        if (!(header->flags & PlatformMemFlag_NoLeakCheck))
        {
            leaked_memory = true;
        }
    }

    if (leaked_memory)
    {
        Win32_ReportError(PlatformError_Nonfatal, "Potential Memory Leak Detected");
    }
}
