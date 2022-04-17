#ifndef TEXTIT_PROJECT_HPP
#define TEXTIT_PROJECT_HPP

typedef uint32_t ProjectFlags;
enum ProjectFlags_ENUM : ProjectFlags
{
    Project_Hidden = 0x1,
};

#define PROJECT_TAG_TABLE_SIZE 4096

struct Project
{
    Project *next;
    Project *prev;

    bool opening;

    int associated_buffer_count;
    ProjectFlags flags;

    uint64_t last_config_write_time;

    String root;

    size_t tag_table_size;
    Tag **tag_table;
};

function void AssociateProject(Buffer *buffer);
function void RemoveProjectAssociation(Buffer *buffer);

function Project *CreateProject(String search_start);
function void DestroyProject(Project *project);

function Project *GetActiveProject();
function Buffer *FindOrOpenBuffer(Project *project, String name);

struct ProjectIterator
{
    Project *project;
};

function ProjectIterator IterateProjects();
function bool IsValid(ProjectIterator *it);
function void Next(ProjectIterator *it);
function void DestroyCurrent(ProjectIterator *it);

#endif /* TEXTIT_PROJECT_HPP */
