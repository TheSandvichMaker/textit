#ifndef TEXTIT_BUFFER_HPP
#define TEXTIT_BUFFER_HPP

enum LineEndKind
{
    LineEnd_LF,
    LineEnd_CRLF,
};

static inline String
LineEndString(LineEndKind kind)
{
    switch (kind)
    {
        case LineEnd_LF:   return StringLiteral("\n");
        case LineEnd_CRLF: return StringLiteral("\r\n");
    }
    return StringLiteral("");
}

#define TEXTIT_BUFFER_SIZE Megabytes(1)
struct Buffer
{
    Arena arena;
    String name;

    LineEndKind line_end;

    int64_t count;
    uint8_t text[TEXTIT_BUFFER_SIZE];
};

struct BufferLocation
{
    int64_t line_number;
    Range line_range;
    int64_t pos;
};

#endif /* TEXTIT_BUFFER_HPP */
