#include "win32_textit.hpp"
#include "textit_string.cpp"

#include <d3d11_1.h>
#include <d3dcompiler.h>

#define CheckHr(hr) if (FAILED(hr)) { goto bail; }
#define CheckedRelease(ptr) if (ptr) { ptr->Release(); ptr = nullptr; }

//
// TODO: Run game on separate thread from the windows guff
// Presumably this requires no longer relying on Dwm for vsync which is fine
// Instead of OpenGL I could do this: https://gist.github.com/mmozeiko/6f36f2b82204b70a9b7fe6c05ccd868f?ts=4
// Although for just showing textures a jank immediate mode OpenGL context would work fine
//

static volatile bool g_running;
static LARGE_INTEGER g_perf_freq;
static WINDOWPLACEMENT g_window_position = { sizeof(g_window_position) };

static Win32State win32_state;
static Platform platform_;

static bool g_use_d3d = false;

function PlatformEventIterator
Win32_IterateEvents(PlatformEventFilter filter)
{
    PlatformEventIterator result = {};
    result.filter = filter;
    result.index  = win32_state.event_read_index;
    return result;
}

function bool
Win32_NextEvent(PlatformEventIterator *it, PlatformEvent *out_event)
{
    bool result = false;

    uint32_t write_index = win32_state.working_write_index;
    while (it->index != write_index)
    {
        int event_index = it->index % ArrayCount(win32_state.events);
        PlatformEvent *event = &win32_state.events[event_index];
        if (!event->consumed_ && MatchFilter(event->type, it->filter))
        {
            result           = true;
            event->consumed_ = true;
            *out_event       = *event;
            break;
        }
        it->index += 1;
    }

    return result;
}

function void
Win32_PushEvent(PlatformEvent *event)
{
    uint32_t read_index  = win32_state.event_read_index;
    uint32_t write_index = win32_state.event_write_index;
    uint32_t size = write_index - read_index;

    if (size < ArrayCount(win32_state.events))
    {
        win32_state.events[write_index % ArrayCount(win32_state.events)] = *event;

        WRITE_BARRIER;

        win32_state.event_write_index = write_index + 1;
    }
}

static void
Win32_PushTickEvent(void)
{
    PlatformEvent event = {};
    event.type = PlatformEvent_Tick;
    Win32_PushEvent(&event);
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

    BeginTicketMutex(&win32_state.allocation_mutex);
    header->next = &win32_state.allocation_sentinel;
    header->prev = win32_state.allocation_sentinel.prev;
    header->next->prev = header;
    header->prev->next = header;
    EndTicketMutex(&win32_state.allocation_mutex);

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
        BeginTicketMutex(&win32_state.allocation_mutex);
        header->prev->next = header->next;
        header->next->prev = header->prev;
        EndTicketMutex(&win32_state.allocation_mutex);
        VirtualFree(header, 0, MEM_RELEASE);
    }
}

function wchar_t *
Win32_FormatError(HRESULT error)
{
    wchar_t *message = NULL;

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

function void
Win32_DisplayLastError(void)
{
    DWORD error = GetLastError() & 0xFFFF;
    wchar_t *message = Win32_FormatError(error);
    MessageBoxW(0, message, L"Error", MB_OK);
    LocalFree(message);
}

function void
Win32_DisplayError(HRESULT hr)
{
    wchar_t *message = Win32_FormatError(hr);
    MessageBoxW(0, message, L"Error", MB_OK);
    LocalFree(message);
}

function void
Win32_ExitWithLastError(int exit_code = -1)
{
    Win32_DisplayLastError();
    ExitProcess(exit_code);
}

function wchar_t *
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

function String_utf16
Win32_Utf8ToUtf16(Arena *arena, String utf8)
{
    String_utf16 result = {};
    if (!utf8.size) return result;

    result.size = (size_t)MultiByteToWideChar(CP_UTF8, 0, (char *)utf8.data, (int)utf8.size, nullptr, 0);
    result.data = PushArrayNoClear(arena, result.size + 1, wchar_t);
    if (result.data)
    {
        if (!MultiByteToWideChar(CP_UTF8, 0, (char *)utf8.data, (int)utf8.size, result.data, (int)result.size))
        {
            Win32_DisplayLastError();
        }

        result.data[result.size] = 0;
    }
    else
    {
        INVALID_CODE_PATH;
    }

    return result;
}

function String
Win32_Utf16ToUtf8(Arena *arena, wchar_t *string, int size = -1)
{
    String result = {};

    result.size = (size_t)WideCharToMultiByte(CP_UTF8, 0, string, size, nullptr, 0, nullptr, nullptr);
    result.data = PushArrayNoClear(arena, result.size + 1, uint8_t);
    if (result.data)
    {
        if (!WideCharToMultiByte(CP_UTF8, 0, string, size, (char *)result.data, (int)result.size, nullptr, nullptr))
        {
            Win32_DisplayLastError();
        }

        result.data[result.size] = 0;
    }
    else
    {
        INVALID_CODE_PATH;
    }

    return result;
}

function String
Win32_Utf16ToUtf8(Arena *arena, String_utf16 utf16)
{
    String result = Win32_Utf16ToUtf8(arena, utf16.data, (int)utf16.size);
    return result;
}

function wchar_t *
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

function wchar_t *
FormatWString(Arena *arena, wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    wchar_t *result = FormatWStringV(arena, fmt, args);
    va_end(args);
    return result;
}

function String
FormatStringV(Arena *arena, char *fmt, va_list args_init)
{
    va_list args_size;
    va_copy(args_size, args_init);

    va_list args_fmt;
    va_copy(args_fmt, args_init);

    String result = {};

    result.size = vsnprintf(nullptr, 0, fmt, args_size);
    va_end(args_size);

    result.data = PushArrayNoClear(arena, result.size + 1, uint8_t);
    vsnprintf((char *)result.data, result.size + 1, fmt, args_fmt);
    va_end(args_fmt);

    return result;
}

function String
FormatString(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = FormatStringV(arena, fmt, args);
    va_end(args);
    return result;
}

function void
Win32_DebugPrint(char *fmt, ...)
{
    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, fmt);
    char *formatted = (char *)FormatStringV(arena, fmt, args).data;
    va_end(args);

    wchar_t *fmt_wide = Win32_Utf8ToUtf16(arena, formatted);

    wprintf(fmt_wide);
    OutputDebugStringW(fmt_wide);
}

function void
Win32_LogPrint(PlatformLogLevel level, char *fmt, ...)
{
    Win32State *state = &win32_state;

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, fmt);
    char *formatted = (char *)FormatStringV(arena, fmt, args).data;
    va_end(args);

    Win32_DebugPrint("LOG MESSAGE: %s\n", formatted);

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

function PlatformLogLine *
Win32_GetFirstLogLine(void)
{
    PlatformLogLine *result = &win32_state.log_lines[win32_state.log_line_first];
    return result;
}

function PlatformLogLine *
Win32_GetLatestLogLine(void)
{
    PlatformLogLine *result = &win32_state.log_lines[(win32_state.log_line_first + win32_state.log_line_count - 1) % PLATFORM_MAX_LOG_LINES];
    return result;
}

function PlatformLogLine *
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

function PlatformLogLine *
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

function void
Win32_ReportError(PlatformErrorType type, char *error, ...)
{
    Arena *arena = platform->GetTempArena();
    ScopedMemory temp(arena);

    va_list args;
    va_start(args, error);
    char *formatted_error = (char *)FormatStringV(arena, error, args).data;
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

function bool
Win32_WriteClipboard(String text)
{
    bool result = false;

    if (text.size <= (size_t)INT32_MAX)
    {
        if (OpenClipboard(win32_state.window))
        {
            if (EmptyClipboard())
            {
                int wchar_count = MultiByteToWideChar(CP_UTF8, 0, (char *)text.data, (int)text.size, nullptr, 0);
                int null_terminated_count = wchar_count + 1;

                HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t)*null_terminated_count);

                wchar_t *memory = (wchar_t *)GlobalLock(handle);
                if (memory)
                {
                    if (!MultiByteToWideChar(CP_UTF8, 0, (char *)text.data, (int)text.size, memory, wchar_count))
                    {
                        Win32_DisplayLastError();
                    }

                    memory[wchar_count] = 0;

                    GlobalUnlock(handle);

                    if (SetClipboardData(CF_UNICODETEXT, handle))
                    {
                        result = true;
                    }
                    else
                    {
                        Win32_DisplayLastError();
                    }
                }
            }
            else
            {
                Win32_DisplayLastError();
            }

            if (!CloseClipboard())
            {
                Win32_DisplayLastError();
            }
        }
        else
        {
            Win32_DisplayLastError();
        }
    }

    return result;
}

function String
Win32_ReadClipboard(Arena *arena)
{
    String result = {};

    if (OpenClipboard(NULL))
    {
        UINT format = 0;
        for (;;)
        {
            format = EnumClipboardFormats(format);

            if (!format)
            {
                if (GetLastError() != ERROR_SUCCESS)
                {
                    Win32_DisplayLastError();
                }

                break;
            }

            if (format == CF_TEXT ||
                format == CF_UNICODETEXT)
            {
                break;
            }
        }

        if (format)
        {
            HANDLE handle = GetClipboardData(format);
            if (handle)
            {
                SIZE_T data_size = GlobalSize(handle);
                wchar_t *data = (wchar_t *)GlobalLock(handle);
                if (data)
                {
                    switch (format)
                    {
                        case CF_TEXT:
                        {
                            result = PushString(arena, MakeString(data_size, (uint8_t *)data));
                        } break;

                        case CF_UNICODETEXT:
                        {
                            result = Win32_Utf16ToUtf8(arena, (wchar_t *)data);
                        } break;
                    }
                    GlobalUnlock(handle);
                }
                else
                {
                    Win32_DisplayLastError();
                }
            }
            else
            {
                Win32_DisplayLastError();
            }
        }

        if (!CloseClipboard())
        {
            Win32_DisplayLastError();
        }
    }
    else
    {
        Win32_DisplayLastError();
    }

    return result;
}

static size_t
Win32_GetFileSize(String filename)
{
    size_t result = 0;

    ScopedMemory temp(platform->GetTempArena());
    wchar_t *file_wide = Win32_Utf8ToUtf16(temp, (char *)filename.data, (int)filename.size);

    HANDLE handle = CreateFileW(file_wide, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        DWORD file_size_high;
        DWORD file_size_low = GetFileSize(handle, &file_size_high);
        CloseHandle(handle);

        LARGE_INTEGER large;
        large.LowPart = file_size_low;
        large.HighPart = file_size_high;

        result = large.QuadPart;
    }

    return result;
}

static bool
Win32_SetWorkingDirectory(String path)
{
    wchar_t *wpath = Win32_Utf8ToUtf16(platform->GetTempArena(), (char *)path.data, (int)path.size);
    bool result = SetCurrentDirectory(wpath);
    return result;
}

static size_t
Win32_ReadFileInto(size_t buffer_size, void *buffer, String filename)
{
    Assert((buffer_size == 0) || buffer);

    size_t result = 0;

    ScopedMemory temp(platform->GetTempArena());
    wchar_t *file_wide = Win32_Utf8ToUtf16(temp, (char *)filename.data, (int)filename.size);

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
        CloseHandle(handle);
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

    ScopedMemory temp(platform->GetTempArena());
    wchar_t *file_wide = Win32_Utf8ToUtf16(temp, (char *)filename.data, (int)filename.size);

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
        CloseHandle(handle);
    }
    else
    {
        Win32_DebugPrint("Could not open file '%s'\n", filename);
    }

    return result;
}

struct Win32FileIterator
{
    PlatformFileIterator it;
    HANDLE handle;
    WIN32_FIND_DATAW find_data;
    char name_buffer[1024];
};

function void
Win32_UpdateFileIterData(PlatformFileIterator *it)
{
    Win32FileIterator *win32_it = (Win32FileIterator *)it;
    it->info.directory = !!(win32_it->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    it->info.name.data = (uint8_t *)win32_it->name_buffer;
    it->info.name.size = snprintf(win32_it->name_buffer, sizeof(win32_it->name_buffer), "%S", win32_it->find_data.cFileName);
}

static bool
Win32_FileIteratorIsValid(PlatformFileIterator *it)
{
    Win32FileIterator *win32_it = (Win32FileIterator *)it;
    return (win32_it->handle != INVALID_HANDLE_VALUE);
}

static void
Win32_FileIteratorNext(PlatformFileIterator *it)
{
    Win32FileIterator *win32_it = (Win32FileIterator *)it;
    if (FindNextFileW(win32_it->handle, &win32_it->find_data))
    {
        Win32_UpdateFileIterData(it);
    }
    else
    {
        win32_it->handle = INVALID_HANDLE_VALUE;
    }
}

static PlatformFileIterator *
Win32_FindFiles(Arena *arena, String query)
{
    Win32FileIterator *win32_it = PushStruct(arena, Win32FileIterator);

    PlatformFileIterator *it = &win32_it->it;

    wchar_t *wquery = FormatWString(platform->GetTempArena(), L"%.*S*", StringExpand(query));
    win32_it->handle = FindFirstFileW(wquery, &win32_it->find_data);

    Win32_UpdateFileIterData(it);

    return &win32_it->it;
}

function void
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

static HDC fuckity_dc;

function void
Win32_ResizeOffscreenBuffer(PlatformOffscreenBuffer *buffer, int32_t w, int32_t h)
{
    Bitmap *bitmap = &buffer->bitmap;
    if (bitmap->data &&
        (w != bitmap->w) ||
        (h != bitmap->h))
    {
        DeleteDC((HDC)buffer->opaque[0]);
        buffer->opaque[0] = NULL;

        DeleteObject(buffer->opaque[1]);
        buffer->opaque[1] = NULL;

        bitmap->data = nullptr;
    }

    if (!bitmap->data &&
        (w > 0) &&
        (h > 0))
    {
        bitmap->w = w;
        bitmap->h = h;
        bitmap->pitch = w;

        BITMAPINFO font_bitmap_info = {};
        BITMAPINFOHEADER *header = &font_bitmap_info.bmiHeader;
        header->biSize        = sizeof(*header);
        header->biWidth       = bitmap->w;
        header->biHeight      = -bitmap->h;
        header->biPlanes      = 1;
        header->biBitCount    = 32;
        header->biCompression = BI_RGB;

        HDC window_dc = GetDC(win32_state.window);
        defer { ReleaseDC(win32_state.window, window_dc); };

        HDC     buffer_dc   = CreateCompatibleDC(window_dc);
        HBITMAP dib_section = CreateDIBSection(fuckity_dc, &font_bitmap_info, DIB_RGB_COLORS, (void **)&bitmap->data, NULL, 0);
        if (!SelectObject(buffer_dc, dib_section))
        {
            INVALID_CODE_PATH;
        }
        buffer->opaque[0] = buffer_dc;
        buffer->opaque[1] = dib_section;
    }
}

function void
Win32_DisplayOffscreenBuffer(HWND window, PlatformOffscreenBuffer *buffer)
{
    HDC dc = GetDC(window);

    RECT client_rect;
    GetClientRect(window, &client_rect);

    int dst_w = client_rect.right;
    int dst_h = client_rect.bottom;

    Bitmap *bitmap = &buffer->bitmap;

    BITMAPINFO bitmap_info = {};
    BITMAPINFOHEADER *bitmap_header = &bitmap_info.bmiHeader;
    bitmap_header->biSize = sizeof(*bitmap_header);
    bitmap_header->biWidth = bitmap->w;
    bitmap_header->biHeight = -bitmap->h;
    bitmap_header->biPlanes = 1;
    bitmap_header->biBitCount = 32;
    bitmap_header->biCompression = BI_RGB;

    StretchDIBits(dc,
                  0, 0, dst_w, dst_h,
                  0, 0, bitmap->w, bitmap->h,
                  bitmap->data,
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
        case WM_QUIT:
        {
            g_running = false;
        } break;

        case WM_SIZING:
        {
            RECT *rect = (RECT *)l_param;

            int w = rect->right - rect->left;
            int h = rect->bottom - rect->top;

            w = platform->window_resize_snap_w*((w + platform->window_resize_snap_w - 1) / platform->window_resize_snap_w);
            h = platform->window_resize_snap_h*((h + platform->window_resize_snap_h - 1) / platform->window_resize_snap_h);

            rect->right = rect->left + w;
            rect->bottom = rect->top + h;

            result = true;
        } break;

        default:
        {
            result = DefWindowProcW(window, message, w_param, l_param);
        } break;
    }
    return result;
}

static HWND
Win32_CreateWindow(HINSTANCE instance, int x, int y, int w, int h, const wchar_t *title, bool no_redirection_bitmap)
{
    if (!win32_state.window_class_registered)
    {
        win32_state.window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
        win32_state.window_class.lpfnWndProc = Win32_WindowProc;
        win32_state.window_class.hInstance = instance;
        win32_state.window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        win32_state.window_class.lpszClassName = L"Win32WindowClass";
        win32_state.window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        win32_state.window_class_registered = RegisterClassW(&win32_state.window_class);
        if (!win32_state.window_class_registered)
        {
            Win32_ExitWithLastError();
        }
    }

    int window_x = x;
    int window_y = y;
    int render_w = w;
    int render_h = h;

    RECT window_rect = { window_x, window_y, window_x + render_w, window_y + render_h };
    AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, false, 0);

    int window_w = window_rect.right - window_rect.left;
    int window_h = window_rect.bottom - window_rect.top;

    DWORD extended_style = 0;
    if (no_redirection_bitmap) extended_style |= WS_EX_NOREDIRECTIONBITMAP;

    HWND window_handle = CreateWindowExW(extended_style,
                                         win32_state.window_class.lpszClassName,
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

struct D3D_State
{
    ID3D11Device           *base_device;
    ID3D11DeviceContext    *base_device_context;
    IDXGIDevice1           *dxgi_device;
    IDXGIAdapter           *dxgi_adapter;
    IDXGIFactory2          *dxgi_factory;
    ID3D11Device1          *device;
    ID3D11DeviceContext1   *device_context;
    IDXGISwapChain1        *swap_chain;
    ID3D11Texture2D        *back_buffer;
    ID3D11Texture2D        *cpu_buffer;

    DWORD display_width, display_height;
} d3d;

function void
D3D_Deinitialize(void)
{
    CheckedRelease(d3d.base_device);
    CheckedRelease(d3d.base_device_context);
    CheckedRelease(d3d.device);
    CheckedRelease(d3d.device_context);
    CheckedRelease(d3d.dxgi_device);
    CheckedRelease(d3d.dxgi_adapter);
    CheckedRelease(d3d.dxgi_factory);
    CheckedRelease(d3d.swap_chain);          
    CheckedRelease(d3d.back_buffer);        
    CheckedRelease(d3d.cpu_buffer);        
}

function bool
D3D_Initialize(HWND window)
{
    bool result = false;

    HRESULT hr;

    {
        D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

        hr = D3D11CreateDevice(nullptr,
                               D3D_DRIVER_TYPE_HARDWARE,
                               nullptr,
                               D3D11_CREATE_DEVICE_DEBUG|D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               feature_levels,
                               ArrayCount(feature_levels),
                               D3D11_SDK_VERSION,
                               &d3d.base_device,
                               nullptr,
                               &d3d.base_device_context);
        CheckHr(hr);
    }

    hr = d3d.base_device->QueryInterface(__uuidof(ID3D11Device1), (void **)&d3d.device);
    CheckHr(hr);

    hr = d3d.base_device_context->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&d3d.device_context);
    CheckHr(hr);

    hr = d3d.device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&d3d.dxgi_device);
    CheckHr(hr);

    hr = d3d.dxgi_device->GetAdapter(&d3d.dxgi_adapter);
    CheckHr(hr);

    hr = d3d.dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&d3d.dxgi_factory);
    CheckHr(hr);

    {
        RECT rect;
        GetClientRect(window, &rect);

        d3d.display_width  = rect.left;
        d3d.display_height = rect.bottom;

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.Width              = d3d.display_width;
        swap_chain_desc.Height             = d3d.display_height;
        swap_chain_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        swap_chain_desc.Stereo             = FALSE;
        swap_chain_desc.SampleDesc.Count   = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount        = 2;
        swap_chain_desc.Scaling            = DXGI_SCALING_NONE;
        swap_chain_desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swap_chain_desc.Flags              = 0;

        hr = d3d.dxgi_factory->CreateSwapChainForHwnd(d3d.device, window, &swap_chain_desc, nullptr, nullptr, &d3d.swap_chain);
        CheckHr(hr);
    }

    hr = d3d.dxgi_factory->MakeWindowAssociation(window, 0);
    CheckHr(hr);

    result = true;

bail:
    if (!result)
    {
        Win32_LogPrint(PlatformLogLevel_Warning, "Failed to initialize D3D11. Falling back to GDI.");
        D3D_Deinitialize();
    }

    return result;
}

function Bitmap
D3D_AcquireCpuBuffer(DWORD width, DWORD height)
{
    Bitmap result = {};

    HRESULT hr;

    if (width  != d3d.display_width ||
        height != d3d.display_height)
    {
        CheckedRelease(d3d.back_buffer);
        CheckedRelease(d3d.cpu_buffer);

        d3d.display_width = width;
        d3d.display_height = height;

        if (width && height)
        {
            hr = d3d.swap_chain->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
            Assert(SUCCEEDED(hr));

            hr = d3d.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&d3d.back_buffer);
            Assert(SUCCEEDED(hr));

            D3D11_TEXTURE2D_DESC tex_desc = {};
            tex_desc.Width              = width;
            tex_desc.Height             = height;
            tex_desc.MipLevels          = 1;
            tex_desc.ArraySize          = 1;
            tex_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
            tex_desc.SampleDesc.Count   = 1;
            tex_desc.SampleDesc.Quality = 0;
            tex_desc.Usage              = D3D11_USAGE_DYNAMIC;
            tex_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
            tex_desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;

            hr = d3d.device->CreateTexture2D(&tex_desc, NULL, &d3d.cpu_buffer);
            Assert(SUCCEEDED(hr));
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped;

    hr = d3d.device_context->Map(d3d.cpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    Assert(SUCCEEDED(hr));

    result.w     = (int)width;
    result.h     = (int)height;
    result.pitch = (int)(mapped.RowPitch / sizeof(Color));
    result.data  = (Color *)mapped.pData;

    return result;
}

function void
D3D_ReleaseCpuBuffer(void)
{
    d3d.device_context->Unmap(d3d.cpu_buffer, 0);
    d3d.device_context->CopyResource(d3d.back_buffer, d3d.cpu_buffer);
}

function void
D3D_Present(void)
{
    HRESULT hr = d3d.swap_chain->Present(1, 0);
    if (hr == DXGI_STATUS_OCCLUDED)
    {
        // window is not visible, so no vsync is possible,
        // sleep a bit instead.
        Sleep(16);
    }
    else
    {
        Assert(SUCCEEDED(hr));
    }
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
                case VK_F11:
                {
                    win32_state.late_latching = !win32_state.late_latching;
                } break;
                case VK_RETURN:
                {
                    if (!g_use_d3d)
                    {
                        Win32_ToggleFullscreen(window);
                    }
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

function bool
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

function uint64_t
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

function void
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

function Arena *
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
    queue->done = CreateEventA(NULL, TRUE, TRUE, NULL);
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
    if (queue->jobs_in_flight > 0)
    {
        WaitForSingleObject(queue->done, INFINITE);
    }
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

static uint8_t codepage_437_utf8[] =
{
    0x0,  0xE2, 0x98, 0xBA, 0xE2, 0x98, 0xBB, 0xE2, 0x99, 0xA5, 0xE2, 0x99, 0xA6, 0xE2, 0x99, 0xA3,
    0xE2, 0x99, 0xA0, 0xE2, 0x80, 0xA2, 0xE2, 0x97, 0x98, 0x9,  0xA,  0xE2, 0x99, 0x82, 0xE2, 0x99,
    0x80, 0xA,  0xE2, 0x99, 0xAB, 0xE2, 0x98, 0xBC, 0xE2, 0x96, 0xBA, 0xE2, 0x97, 0x84, 0xE2, 0x86,
    0x95, 0xE2, 0x80, 0xBC, 0xC2, 0xB6, 0xC2, 0xA7, 0xE2, 0x96, 0xAC, 0xE2, 0x86, 0xA8, 0xE2, 0x86,
    0x91, 0xE2, 0x86, 0x93, 0xE2, 0x86, 0x92, 0xE2, 0x86, 0x90, 0xE2, 0x88, 0x9F, 0xE2, 0x86, 0x94,
    0xE2, 0x96, 0xB2, 0xE2, 0x96, 0xBC, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0xE2, 0x8C, 0x82, 0xC3, 0x87, 0xC3, 0xBC, 0xC3, 0xA9, 0xC3, 0xA2,
    0xC3, 0xA4, 0xC3, 0xA0, 0xC3, 0xA5, 0xC3, 0xA7, 0xC3, 0xAA, 0xC3, 0xAB, 0xC3, 0xA8, 0xC3, 0xAF,
    0xC3, 0xAE, 0xC3, 0xAC, 0xC3, 0x84, 0xC3, 0x85, 0xC3, 0x89, 0xC3, 0xA6, 0xC3, 0x86, 0xC3, 0xB4,
    0xC3, 0xB6, 0xC3, 0xB2, 0xC3, 0xBB, 0xC3, 0xB9, 0xC3, 0xBF, 0xC3, 0x96, 0xC3, 0x9C, 0xC2, 0xA2,
    0xC2, 0xA3, 0xC2, 0xA5, 0xE2, 0x82, 0xA7, 0xC6, 0x92, 0xC3, 0xA1, 0xC3, 0xAD, 0xC3, 0xB3, 0xC3,
    0xBA, 0xC3, 0xB1, 0xC3, 0x91, 0xC2, 0xAA, 0xC2, 0xBA, 0xC2, 0xBF, 0xE2, 0x8C, 0x90, 0xC2, 0xAC,
    0xC2, 0xBD, 0xC2, 0xBC, 0xC2, 0xA1, 0xC2, 0xAB, 0xC2, 0xBB, 0xE2, 0x96, 0x91, 0xE2, 0x96, 0x92,
    0xE2, 0x96, 0x93, 0xE2, 0x94, 0x82, 0xE2, 0x94, 0xA4, 0xE2, 0x95, 0xA1, 0xE2, 0x95, 0xA2, 0xE2,
    0x95, 0x96, 0xE2, 0x95, 0x95, 0xE2, 0x95, 0xA3, 0xE2, 0x95, 0x91, 0xE2, 0x95, 0x97, 0xE2, 0x95,
    0x9D, 0xE2, 0x95, 0x9C, 0xE2, 0x95, 0x9B, 0xE2, 0x94, 0x90, 0xE2, 0x94, 0x94, 0xE2, 0x94, 0xB4,
    0xE2, 0x94, 0xAC, 0xE2, 0x94, 0x9C, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0xBC, 0xE2, 0x95, 0x9E, 0xE2,
    0x95, 0x9F, 0xE2, 0x95, 0x9A, 0xE2, 0x95, 0x94, 0xE2, 0x95, 0xA9, 0xE2, 0x95, 0xA6, 0xE2, 0x95,
    0xA0, 0xE2, 0x95, 0x90, 0xE2, 0x95, 0xAC, 0xE2, 0x95, 0xA7, 0xE2, 0x95, 0xA8, 0xE2, 0x95, 0xA4,
    0xE2, 0x95, 0xA5, 0xE2, 0x95, 0x99, 0xE2, 0x95, 0x98, 0xE2, 0x95, 0x92, 0xE2, 0x95, 0x93, 0xE2,
    0x95, 0xAB, 0xE2, 0x95, 0xAA, 0xE2, 0x94, 0x98, 0xE2, 0x94, 0x8C, 0xE2, 0x96, 0x88, 0xE2, 0x96,
    0x84, 0xE2, 0x96, 0x8C, 0xE2, 0x96, 0x90, 0xE2, 0x96, 0x80, 0xCE, 0xB1, 0xC3, 0x9F, 0xCE, 0x93,
    0xCF, 0x80, 0xCE, 0xA3, 0xCF, 0x83, 0xC2, 0xB5, 0xCF, 0x84, 0xCE, 0xA6, 0xCE, 0x98, 0xCE, 0xA9,
    0xCE, 0xB4, 0xE2, 0x88, 0x9E, 0xCF, 0x86, 0xCE, 0xB5, 0xE2, 0x88, 0xA9, 0xE2, 0x89, 0xA1, 0xC2,
    0xB1, 0xE2, 0x89, 0xA5, 0xE2, 0x89, 0xA4, 0xE2, 0x8C, 0xA0, 0xE2, 0x8C, 0xA1, 0xC3, 0xB7, 0xE2,
    0x89, 0x88, 0xC2, 0xB0, 0xE2, 0x88, 0x99, 0xC2, 0xB7, 0xE2, 0x88, 0x9A, 0xE2, 0x81, 0xBF, 0xC2,
    0xB2, 0xE2, 0x96, 0xA0, 0xC2, 0xA0, 
};

static wchar_t codepage_437_utf16[] =
{
    0x0,    0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x9,    0xA,    0x2642, 0x2640, 0xA,    0x266B, 0x263C, 
    0x25BA, 0x25C4, 0x2195, 0x203C, 0xB6,   0xA7,   0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,   0x28,   0x29,   0x2A,   0x2B,   0x2C,   0x2D,   0x2E,   0x2F,
    0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,   0x38,   0x39,   0x3A,   0x3B,   0x3C,   0x3D,   0x3E,   0x3F,
    0x40,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,   0x48,   0x49,   0x4A,   0x4B,   0x4C,   0x4D,   0x4E,   0x4F,
    0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,   0x58,   0x59,   0x5A,   0x5B,   0x5C,   0x5D,   0x5E,   0x5F,
    0x60,   0x61,   0x62,   0x63,   0x64,   0x65,   0x66,   0x67,   0x68,   0x69,   0x6A,   0x6B,   0x6C,   0x6D,   0x6E,   0x6F,
    0x70,   0x71,   0x72,   0x73,   0x74,   0x75,   0x76,   0x77,   0x78,   0x79,   0x7A,   0x7B,   0x7C,   0x7D,   0x7E,   0x2302,
    0xC7,   0xFC,   0xE9,   0xE2,   0xE4,   0xE0,   0xE5,   0xE7,   0xEA,   0xEB,   0xE8,   0xEF,   0xEE,   0xEC,   0xC4,   0xC5,
    0xC9,   0xE6,   0xC6,   0xF4,   0xF6,   0xF2,   0xFB,   0xF9,   0xFF,   0xD6,   0xDC,   0xA2,   0xA3,   0xA5,   0x20A7, 0x192,
    0xE1,   0xED,   0xF3,   0xFA,   0xF1,   0xD1,   0xAA,   0xBA,   0xBF,   0x2310, 0xAC,   0xBD,   0xBC,   0xA1,   0xAB,   0xBB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x3B1,  0xDF,   0x393,  0x3C0,  0x3A3,  0x3C3,  0xB5,   0x3C4,  0x3A6,  0x398,  0x3A9,  0x3B4,  0x221E, 0x3C6,  0x3B5,  0x2229,
    0x2261, 0xB1,   0x2265, 0x2264, 0x2320, 0x2321, 0xF7,   0x2248, 0xB0,   0x2219, 0xB7,   0x221A, 0x207F, 0xB2,   0x25A0, 0xA0, 
};

static wchar_t codepage_437_ascii[] =
{
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
    16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
    32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
    48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
    64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
    80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
    96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

function bool
Win32_RegisterFontFile(String file_name_utf8)
{
    String_utf16 file_name = Win32_Utf8ToUtf16(platform->GetTempArena(), file_name_utf8);
    bool result = !!AddFontResourceExW(file_name.data, FR_PRIVATE, 0);
    return result;
}

function PlatformFontHandle
Win32_CreateFont(String font_name_utf8, PlatformFontRasterFlags flags, int height)
{
    String_utf16 font_name = Win32_Utf8ToUtf16(platform->GetTempArena(), font_name_utf8);

    bool raster_font = !!(flags & PlatformFontRasterFlag_RasterFont);

    DWORD out_precision = raster_font ? OUT_RASTER_PRECIS : OUT_DEFAULT_PRECIS;
    DWORD quality       = raster_font ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;

    HFONT font = CreateFontW(height, 0, 0, 0,
                             FW_NORMAL,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             out_precision,
                             CLIP_DEFAULT_PRECIS,
                             quality,
                             DEFAULT_PITCH|FF_DONTCARE,
                             font_name.data);

    PlatformFontHandle handle;
    handle.opaque = font;

    return handle;
}

function void
Win32_DestroyFont(PlatformFontHandle handle)
{
    HFONT font = (HFONT)handle.opaque;
    DeleteObject(font);
}

function V2i
Win32_GetFontMetrics(PlatformFontHandle handle)
{
    V2i result = { -1, -1 };

    HDC dc = GetDC(win32_state.window);
    defer { ReleaseDC(win32_state.window, dc); };

    HFONT font = (HFONT)handle.opaque;
    SelectObject(dc, font);

    SIZE size;
    if (GetTextExtentPoint32W(dc, L"M", 1, &size))
    {
        result.x = size.cx;
        result.y = size.cy;
    }

    return result;
}

function void
Win32_SetTextClipRect(PlatformOffscreenBuffer *target, Rect2i rect)
{
    if (win32_state.current_text_clip_rect)
    {
        DeleteObject(win32_state.current_text_clip_rect);
    }
    win32_state.current_text_clip_rect = CreateRectRgn((int)rect.min.x, (int)rect.min.y,
                                                       (int)rect.max.x, (int)rect.max.y);
    HDC dc = (HDC)target->opaque[0];
    if (!SelectClipRgn(dc, win32_state.current_text_clip_rect))
    {
        INVALID_CODE_PATH;
    }
}

function V2i
Win32_DrawText(PlatformFontHandle handle, PlatformOffscreenBuffer *target, String text, V2i p, Color foreground, Color background)
{
    HDC   dc   = (HDC)target->opaque[0];
    HFONT font = (HFONT)handle.opaque;

    String_utf16 wtext = Win32_Utf8ToUtf16(platform->GetTempArena(), text);

    if (!SelectObject(dc, font))
    {
        INVALID_CODE_PATH;
    }

    if (SetTextAlign(dc, TA_TOP|TA_LEFT) == GDI_ERROR)
    {
        INVALID_CODE_PATH;
    }

    if (SetTextColor(dc, RGB(foreground.r, foreground.g, foreground.b)) == CLR_INVALID)
    {
        INVALID_CODE_PATH;
    }

    UNUSED_VARIABLE(background);
    SetBkMode(dc, TRANSPARENT);
//    if (SetBkColor(dc, RGB(background.r, background.g, background.b)) == CLR_INVALID)  
//    {
//        INVALID_CODE_PATH;
//    }

    if (!TextOutW(dc, (int)p.x, (int)p.y, wtext.data, (int)wtext.size))
    {
        INVALID_CODE_PATH;
    }
    
    SIZE size;
    GetTextExtentPoint32W(dc, wtext.data, (int)wtext.size, &size);

    p.x += size.cx;

    return p;
}

function bool
Win32_MakeAsciiFont(String font_name_utf8, Font *out_font, int font_size, PlatformFontRasterFlags flags)
{
    bool result = false;

    bool raster_font = !!(flags & PlatformFontRasterFlag_RasterFont);
    bool dont_map_unicode = !!(flags & PlatformFontRasterFlag_DoNotMapUnicode);

    ZeroStruct(out_font);

    HDC screen_dc = GetDC(NULL);
    Assert(screen_dc);

    HDC font_dc = CreateCompatibleDC(screen_dc);
    Assert(font_dc);

    ReleaseDC(NULL, screen_dc);

    String_utf16 font_name = Win32_Utf8ToUtf16(platform->GetTempArena(), font_name_utf8);

    int height = font_size;

    DWORD out_precision = raster_font ? OUT_RASTER_PRECIS : OUT_DEFAULT_PRECIS;
    DWORD quality       = raster_font ? NONANTIALIASED_QUALITY : PROOF_QUALITY;

    HFONT font = CreateFontW(height, 0, 0, 0,
                             FW_NORMAL,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             out_precision,
                             CLIP_DEFAULT_PRECIS,
                             quality,
                             DEFAULT_PITCH|FF_DONTCARE,
                             font_name.data);

    wchar_t *charset = (dont_map_unicode ? codepage_437_ascii : codepage_437_utf16);

    if (font)
    {
        if (!SelectObject(font_dc, font))
        {
            INVALID_CODE_PATH;
        }

        SIZE size;
        if (!GetTextExtentPoint32W(font_dc, L"M", 1, &size))
        {
            INVALID_CODE_PATH;
        }

        BITMAPINFO font_bitmap_info = {};
        BITMAPINFOHEADER *header = &font_bitmap_info.bmiHeader;
        header->biSize        = sizeof(*header);
        header->biWidth       = 16*size.cx;
        header->biHeight      = -16*size.cy;
        header->biPlanes      = 1;
        header->biBitCount    = 32;
        header->biCompression = BI_RGB;

        VOID *font_bits = nullptr;
        HBITMAP font_bitmap = CreateDIBSection(font_dc, &font_bitmap_info, DIB_RGB_COLORS, &font_bits, NULL, 0);
        if (font_bitmap)
        {
            SelectObject(font_dc, font_bitmap);
            SetBkColor(font_dc, RGB(0, 0, 0));
            SetTextAlign(font_dc, TA_TOP|TA_LEFT);

            out_font->glyph_w        = size.cx;
            out_font->glyph_h        = size.cy;
            out_font->glyphs_per_row = 16;
            out_font->glyphs_per_col = 16;
            out_font->w              = 16*size.cx;
            out_font->h              = 16*size.cy;
            out_font->pitch          = out_font->w;
            out_font->data           = (Color *)font_bits;

            SetTextColor(font_dc, RGB(255, 255, 255));
            for (int i = 0; i < 255; i += 1)
            {
                int x = i % 16;
                int y = i / 16;
                TextOutW(font_dc, out_font->glyph_w*x, out_font->glyph_h*y, charset + i, 1);
            }

            Color *pixels = out_font->data;
            for (int i = 0; i < out_font->pitch*out_font->h; i += 1)
            {
                pixels[i].a = pixels[i].r;
            }

            result = true;
        }

        DeleteObject(font);
    }
    else
    {
        platform->ReportError(PlatformError_Nonfatal, "Failed to load font '%.*s'", StringExpand(font_name_utf8));
    }

    DeleteDC(font_dc);

    return result;
}

static void
Win32_LoadDefaultFonts()
{
    wchar_t *font_folder = FormatWString(platform->GetTempArena(), L"\\\\?\\%s\\fonts\\*", win32_state.exe_folder);

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(font_folder, &find_data);
    if (handle != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            if (find_data.cFileName[0] != '.')
            {
                wchar_t *full_path = FormatWString(platform->GetTempArena(), L"\\\\?\\%s\\fonts\\%s", win32_state.exe_folder, find_data.cFileName);
                bool succeeded = AddFontResourceExW(full_path, FR_PRIVATE, 0);
                if (!succeeded)
                {
                    platform->DebugPrint("%S failed to load as a font\n", find_data.cFileName);
                }
            }
            if (!FindNextFileW(handle, &find_data))
            {
                break;
            }
        }
    }
}

static DWORD WINAPI
Win32_AppThread(LPVOID userdata)
{
    UNUSED_VARIABLE(userdata);

    HWND window = win32_state.window;

    ThreadLocalContext tls_context = {};
    Win32_InitializeTLSForThread(&tls_context);

    win32_state.exe_folder = FindExeFolderLikeAMonkeyInAMonkeySuit();
    win32_state.dll_path   = FormatWString(&win32_state.arena, L"\\\\?\\%s\\textit.dll", win32_state.exe_folder);

    Win32AppCode *app_code = &win32_state.app_code;
    if (!Win32_LoadAppCode(app_code))
    {
        platform->ReportError(PlatformError_Fatal, "Could not load app code");
    }
     platform->exe_reloaded = true;

     Win32_LoadDefaultFonts();

    HCURSOR arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);

    BOOL composition_enabled;
    if (DwmIsCompositionEnabled(&composition_enabled) != S_OK)
    {
        Win32_DisplayLastError();
    }

    double smooth_frametime = 1.0f / 60.0f;
    PlatformHighResTime frame_start_time = Win32_GetTime();

    platform->dt = 1.0f / 60.0f;

    while (g_running)
    {
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
        platform->mouse_y = cursor_pos.y;
        platform->mouse_y_flipped = client_h - cursor_pos.y - 1;
        platform->mouse_in_window = (platform->mouse_x >= 0 && platform->mouse_x < client_w &&
                                     platform->mouse_y >= 0 && platform->mouse_y < client_h);

        if (platform->mouse_in_window)
        {
            SetCursor(arrow_cursor);
        }

        if (g_use_d3d)
        {
            platform->backbuffer.bitmap = D3D_AcquireCpuBuffer(platform->render_w, platform->render_h);
        }
        else
        {
            Win32_ResizeOffscreenBuffer(&platform->backbuffer, platform->render_w, platform->render_h);
        }

        win32_state.working_write_index = win32_state.event_write_index;

        bool updated = false;
        if (app_code->valid && win32_state.event_read_index != win32_state.working_write_index)
        {
            updated = true;

            ThreadLocalContext *context = &tls_context;
            Swap(context->temp_arena, context->prev_temp_arena);
            Clear(context->temp_arena);

            PlatformHighResTime start = platform->GetTime();

            app_code->UpdateAndRender(platform);
            platform->exe_reloaded = false;

            PlatformHighResTime end = platform->GetTime();
            double time = platform->SecondsElapsed(start, end);

            if (win32_state.update_time_sample_count < (1 << 16))
            {
                win32_state.update_time_sample_count += 1;
                win32_state.update_time_accumulator  += time;
            }
        }

        win32_state.event_read_index = win32_state.working_write_index;

        if (g_use_d3d)
        {
            D3D_ReleaseCpuBuffer();
            D3D_Present();
        }
        else
        {
            Win32_DisplayOffscreenBuffer(window, &platform->backbuffer);

            if (composition_enabled)
            {
                DwmFlush();
            }
        }

        if (win32_state.late_latching &&
            win32_state.update_time_sample_count > 8)
        {
            double refresh_rate = 1.0 / 60.0; // TODO: get actual refresh rate
            double average_update_time = (double)win32_state.update_time_accumulator / (double)win32_state.update_time_sample_count;
            double coarse_safety_ms = 3.0;
            double slop_ms = 2.0;
            double total_sleep = refresh_rate - average_update_time - slop_ms / 1000.0;

            double coarse_sleep_ms = 1000.0*total_sleep - coarse_safety_ms;
            if (updated && coarse_sleep_ms >= 1.0)
            {
                PlatformHighResTime start_time = Win32_GetTime();

                platform->DebugPrint("Late latching, coarse: %ums, fine: %fms\n", (DWORD)coarse_sleep_ms, 1000.0*total_sleep);
                Sleep((DWORD)coarse_sleep_ms);

                for (;;)
                {
                    PlatformHighResTime end_time = Win32_GetTime();
                    if (Win32_SecondsElapsed(start_time, end_time) >= total_sleep)
                    {
                        break;
                    }
                }
            }
        }

        PlatformHighResTime frame_end_time = Win32_GetTime();
        double seconds_elapsed = Win32_SecondsElapsed(frame_start_time, frame_end_time);
        Swap(frame_start_time, frame_end_time);

        smooth_frametime = 0.9f*smooth_frametime + 0.1f*seconds_elapsed;

        String_utf16 user_title = Win32_Utf8ToUtf16(platform->GetTempArena(), platform->window_title);

        wchar_t *title = FormatWString(platform->GetTempArena(),
                                       L"%.*s - frame time: %fms, fps: %f\n - late latching: %s",
                                       StringExpand(user_title),
                                       1000.0*smooth_frametime,
                                       1.0 / smooth_frametime,
                                       win32_state.late_latching ? L"yes" : L"no");
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

        if (platform->exit_requested)
        {
            DestroyWindow(window);
        }
    }

    return 0;
}

int
main(int, char **)
// WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_cmd)
{
    // UNUSED_VARIABLE(command_line);
    // UNUSED_VARIABLE(prev_instance);

    HINSTANCE instance = NULL;

    platform = &platform_;

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    QueryPerformanceFrequency(&g_perf_freq);

    win32_state.allocation_sentinel.next = &win32_state.allocation_sentinel;
    win32_state.allocation_sentinel.prev = &win32_state.allocation_sentinel;

    PlatformJobQueue high_priority_queue = {};
    PlatformJobQueue  low_priority_queue = {};

    win32_state.late_latching = true;

    platform->IterateEvents          = Win32_IterateEvents;
    platform->NextEvent              = Win32_NextEvent;
    platform->PushTickEvent          = Win32_PushTickEvent;

    platform->DebugPrint             = Win32_DebugPrint;
    platform->LogPrint               = Win32_LogPrint;
    platform->GetFirstLogLine        = Win32_GetFirstLogLine;
    platform->GetLatestLogLine       = Win32_GetLatestLogLine;
    platform->GetNextLogLine         = Win32_GetNextLogLine;
    platform->GetPrevLogLine         = Win32_GetPrevLogLine;

    platform->high_priority_queue    = &high_priority_queue;
    platform->low_priority_queue     = &low_priority_queue;

    platform->page_size              = system_info.dwPageSize;
    platform->allocation_granularity = system_info.dwAllocationGranularity;
    platform->ReportError            = Win32_ReportError;
    platform->AllocateMemory         = Win32_Allocate;
    platform->ReserveMemory          = Win32_Reserve;
    platform->CommitMemory           = Win32_Commit;
    platform->DecommitMemory         = Win32_Decommit;
    platform->DeallocateMemory       = Win32_Deallocate;

    platform->RegisterFontFile       = Win32_RegisterFontFile;
    platform->MakeAsciiFont          = Win32_MakeAsciiFont;

    platform->CreateFont             = Win32_CreateFont;
    platform->DestroyFont            = Win32_DestroyFont;
    platform->GetFontMetrics         = Win32_GetFontMetrics;
    platform->SetTextClipRect        = Win32_SetTextClipRect;
    platform->DrawText               = Win32_DrawText;

    platform->GetThreadLocalContext  = Win32_GetThreadLocalContext;
    platform->GetTempArena           = Win32_GetTempArena;

    platform->AddJob                 = Win32_AddJob;
    platform->WaitForJobs            = Win32_WaitForJobs;

    platform->SetWorkingDirectory    = Win32_SetWorkingDirectory;
    platform->ReadFile               = Win32_ReadFile;
    platform->ReadFileInto           = Win32_ReadFileInto;
    platform->GetFileSize            = Win32_GetFileSize;

    platform->FindFiles              = Win32_FindFiles;
    platform->FileIteratorIsValid    = Win32_FileIteratorIsValid;
    platform->FileIteratorNext       = Win32_FileIteratorNext;

    platform->GetTime                = Win32_GetTime;
    platform->SecondsElapsed         = Win32_SecondsElapsed;

    platform->WriteClipboard         = Win32_WriteClipboard;
    platform->ReadClipboard          = Win32_ReadClipboard;

    platform->SleepThread            = Win32_SleepThread;

    //

    win32_state.thread_local_index = TlsAlloc();
    if (win32_state.thread_local_index == TLS_OUT_OF_INDEXES)
    {
        Win32_ExitWithLastError();
    }

    //

    ThreadLocalContext tls_context = {};
    Win32_InitializeTLSForThread(&tls_context);

    Win32_InitializeJobQueue(&high_priority_queue, 8);
    Win32_InitializeJobQueue(&low_priority_queue, 4);

    platform->window_resize_snap_w = 1;
    platform->window_resize_snap_h = 1;

    HWND window = Win32_CreateWindow(instance, 32, 32, 1280, 960, L"Textit", g_use_d3d);
    if (!window)
    {
        platform->ReportError(PlatformError_Fatal, "Could not create window");
    }

    if (g_use_d3d)
    {
        g_use_d3d = D3D_Initialize(window);
        if (!g_use_d3d)
        {
            DestroyWindow(window);
            window = Win32_CreateWindow(instance, 32, 32, 720, 480, L"Textit", false);
            if (!window)
            {
                platform->ReportError(PlatformError_Fatal, "Could not create window");
            }
        }
    }

    win32_state.window = window;

    g_running = true;

    SetFocus(window);
    ShowWindow(window, SW_SHOW);

    WRITE_BARRIER;

    HANDLE app_thread_handle = CreateThread(0, 0, Win32_AppThread, nullptr, 0, nullptr);
    CloseHandle(app_thread_handle);

    wchar_t last_char = 0;

    Win32_PushTickEvent();

    MSG message;
    while (g_running && GetMessageW(&message, 0, 0, 0))
    {
        // TODO: seems a bit redundant in some ways
        {
            ThreadLocalContext *context = &tls_context;
            Swap(context->temp_arena, context->prev_temp_arena);
            Clear(context->temp_arena);
        }

        bool exit_requested = false;

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
                bool pressed = (message.message == WM_KEYDOWN ||
                                message.message == WM_SYSKEYDOWN);
                bool alt_down = (message.lParam & (1 << 29)) != 0;
                bool ctrl_down = !!(GetKeyState(VK_CONTROL) & 0xFF00);
                bool shift_down = !!(GetKeyState(VK_SHIFT) & 0xFF00);

                if (!Win32_HandleSpecialKeys(window, vk_code, pressed, alt_down))
                {
                    PlatformEvent event = {};
                    event.type = (pressed ? PlatformEvent_KeyDown : PlatformEvent_KeyUp);
                    event.pressed = pressed;
                    // TODO: event.repeat
                    event.alt_down = alt_down;
                    event.ctrl_down = ctrl_down;
                    event.shift_down = shift_down;
                    event.input_code = (PlatformInputCode)vk_code; // I gave myself a 1:1 mapping of VK codes to platform input codes, so that's nice.
                    Win32_PushEvent(&event);

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
                PlatformEvent event = {};

                event.pressed = (message.message == WM_LBUTTONDOWN ||
                                 message.message == WM_MBUTTONDOWN ||
                                 message.message == WM_RBUTTONDOWN ||
                                 message.message == WM_XBUTTONDOWN);
                event.type = (event.pressed ? PlatformEvent_MouseDown : PlatformEvent_MouseUp);
                event.ctrl_down = !!(GetKeyState(VK_CONTROL) & 0xFF00);
                event.shift_down = !!(GetKeyState(VK_SHIFT) & 0xFF00);
                switch (message.message)
                {
                    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                    {
                        event.input_code = PlatformInputCode_LButton;
                    } break;
                    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                    {
                        event.input_code = PlatformInputCode_MButton;
                    } break;
                    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                    {
                        event.input_code = PlatformInputCode_RButton;
                    } break;
                    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                    {
                        if (GET_XBUTTON_WPARAM(message.wParam) == XBUTTON1)
                        {
                            event.input_code = PlatformInputCode_XButton1;
                        }
                        else
                        {
                            Assert(GET_XBUTTON_WPARAM(message.wParam) == XBUTTON2);
                            event.input_code = PlatformInputCode_XButton2;
                        }
                    } break;
                }

                Win32_PushEvent(&event);
            } break;

            case WM_MOUSEMOVE:
            {
                PlatformEvent event = {};
                event.type = PlatformEvent_MouseMove;
                event.ctrl_down  = !!(message.wParam & MK_CONTROL);
                event.shift_down = !!(message.wParam & MK_SHIFT);
                event.pos_x = LOWORD(message.lParam);
                event.pos_y = HIWORD(message.lParam);
                Win32_PushEvent(&event);
            } break;

            case WM_CHAR:
            {
                PlatformEvent event = {};

                event.ctrl_down = !!(GetKeyState(VK_CONTROL) & 0xFF00);
                event.shift_down = !!(GetKeyState(VK_SHIFT) & 0xFF00);

                wchar_t chars[3] = {};

                wchar_t curr_char = (wchar_t)message.wParam;
                if (IS_HIGH_SURROGATE(curr_char))
                {
                    last_char = curr_char;
                }
                else if (IS_LOW_SURROGATE(curr_char))
                {
                    if (IS_SURROGATE_PAIR(last_char, curr_char))
                    {
                        chars[0] = last_char;
                        chars[1] = curr_char;
                    }
                    last_char = 0;
                }
                else
                {
                    chars[0] = curr_char;
                    if (chars[0] == L'\r')
                    {
                        chars[0] = L'\n';
                    }
                }
                event.type = PlatformEvent_Text;
                event.text = Win32_Utf16ToUtf8(platform->GetTempArena(), chars);

                Win32_PushEvent(&event);
            } break;

            case WM_IME_CHAR:
            {
                platform->DebugPrint("Got WM_IME_CHAR:\n");
            } break;

            default:
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            } break;
        }

        exit_requested |= platform->exit_requested;

        if (exit_requested)
        {
            g_running = false;
        }
    }

    Win32_CloseJobQueue(platform->high_priority_queue);
    Win32_CloseJobQueue(platform->low_priority_queue);

    if (!g_use_d3d)
    {
        Win32_ResizeOffscreenBuffer(&platform->backbuffer, 0, 0);
    }

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

#if 0
    if (leaked_memory)
    {
        Win32_ReportError(PlatformError_Nonfatal, "Potential Memory Leak Detected");
    }
#endif

    ExitProcess(0);
}
