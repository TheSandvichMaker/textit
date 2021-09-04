#ifndef TEXTIT_PROJECT_HPP
#define TEXTIT_PROJECT_HPP

typedef uint32_t ProjectFlags;
enum ProjectFlags_ENUM : ProjectFlags
{
    Project_Hidden = 0x1,
};

struct Project
{
    Project *next;

    bool opening;

    int associated_buffer_count;
    ProjectFlags flags;

    String root;

    TicketMutex tag_table_mutex;
    Tag *tag_table[4096];
};

#endif /* TEXTIT_PROJECT_HPP */
