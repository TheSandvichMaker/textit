function String
FindProjectRoot(Arena *arena, String search_start)
{
    ScopedMemory temp;

    String search_path = SplitPath(search_start);
    for (int i = 0; i < 16; i += 1)
    {
        bool found_any_code = false;

        for (PlatformFileIterator *it = platform->FindFiles(temp, search_path);
             platform->FileIteratorIsValid(it);
             platform->FileIteratorNext(it))
        {
            if (it->info.directory) continue;

            String ext;
            SplitExtension(it->info.name, &ext);

            if (GetLanguageFromExtension(ext))
            {
                found_any_code = true;
                break;
            }
        }

        if (!found_any_code)
        {
            break;
        }

        // WARNING: very lazy
        search_path = PushTempStringF("%.*s..\\", StringExpand(search_path));
    }

    String result = platform->PushFullPath(arena, search_path);
    return result;
}

function void
OpenCodeFilesRecursively(String search_start, BufferFlags buffer_flags = 0)
{
    Arena *temp = platform->GetTempArena();

    String search_path = SplitPath(search_start);
    for (PlatformFileIterator *it = platform->FindFiles(temp, search_path);
         platform->FileIteratorIsValid(it);
         platform->FileIteratorNext(it))
    {
        if (AreEqual(it->info.name, "."_str) || AreEqual(it->info.name, ".."_str)) continue; // first two are always '.' and '..', not sure what to do with that yet
        if (AreEqual(it->info.name, ".vs"_str)) continue; // no.
        if (AreEqual(it->info.name, ".git"_str)) continue; // don't do it.

        if (it->info.directory)
        {
            OpenCodeFilesRecursively(PushTempStringF("%.*s%.*s\\", StringExpand(search_path), StringExpand(it->info.name)), buffer_flags);
        }
        else
        {
            String ext;
            SplitExtension(it->info.name, &ext);

            if (GetLanguageFromExtension(ext))
            {
                OpenBufferFromFileAsync(platform->high_priority_queue, 
                                        PushStringF(temp, "%.*s%.*s", StringExpand(search_path), StringExpand(it->info.name)), 
                                        buffer_flags);
            }
        }
    }
}

function Buffer *
RecursivelyFindAndOpenBuffer(String search_path, String name)
{
    Buffer *result = nullptr;

    ScopedMemory temp;
    for (PlatformFileIterator *it = platform->FindFiles(temp, search_path);
         platform->FileIteratorIsValid(it);
         platform->FileIteratorNext(it))
    {
        if (AreEqual(it->info.name, "."_str) || AreEqual(it->info.name, ".."_str)) continue; // first two are always '.' and '..', not sure what to do with that yet
        if (AreEqual(it->info.name, ".vs"_str)) continue; // no.
        if (AreEqual(it->info.name, ".git"_str)) continue; // don't do it.

        if (it->info.directory)
        {
            RecursivelyFindAndOpenBuffer(PushTempStringF("%.*s%.*s\\", StringExpand(search_path), StringExpand(it->info.name)), name);
        }
        else if (AreEqual(it->info.name, name, StringMatch_CaseInsensitive))
        {
            result = OpenBufferFromFile(PushTempStringF("%.*s%.*s", StringExpand(search_path), StringExpand(it->info.name)));
            break;
        }
    }

    return result;
}

function Buffer *
FindOrOpenBuffer(Project *project, String name)
{
    // TODO: A better heuristic is needed to make sure we pick the right file
    // based on absolute path and all that.

    Buffer *result = nullptr;

    for (BufferIterator it = IterateBuffers();
         IsValid(&it);
         Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (AreEqual(name, buffer->name, StringMatch_CaseInsensitive))
        {
            result = buffer;
            break;
        }
    }

    if (!result)
    {
        result = RecursivelyFindAndOpenBuffer(project->root, name);
    }

    return result;
}

function Project *
CreateProject(String search_start)
{
    Project *project = PushStruct(&editor->transient_arena, Project);
    project->tag_table = PushArray(&editor->transient_arena, PROJECT_TAG_TABLE_SIZE, Tag *);
    project->root = FindProjectRoot(&editor->transient_arena, search_start);

    bool is_first_project = DllIsEmpty(&editor->project_sentinel);
    if (!is_first_project)
    {
        project->flags |= Project_Hidden;
    }
    DllInsertBack(&editor->project_sentinel, project);

    PlatformHighResTime start = platform->GetTime();

    project->opening = true;

    OpenCodeFilesRecursively(project->root);
    platform->WaitForJobs(platform->high_priority_queue);

    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (buffer->project != project) continue;

        Tags *tags = buffer->tags;
        for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
        {
            Tag **slot = &project->tag_table[tag->hash.u32[0] % PROJECT_TAG_TABLE_SIZE];
            tag->next_in_hash = *slot;
            *slot = tag;
        }
    }

    project->opening = false;

    PlatformHighResTime end = platform->GetTime();

    double total_time = platform->SecondsElapsed(start, end);

    size_t total_bytes         = 0;
    size_t total_lines_of_code = 0;
    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (buffer->project != project) continue;

        total_bytes         += buffer->count;
        total_lines_of_code += GetLineCount(buffer);
    }

    platform->DebugPrint("Loaded %s worth of text representing %zu lines of code in %fms\n",
                         FormatHumanReadableBytes(total_bytes).data,
                         total_lines_of_code,
                         1000.0*total_time);
    
    return project;
}

function void
DestroyProject(Project *project)
{
    Assert(project);
    Assert(project != &editor->project_sentinel);

    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        Buffer *buffer = it.buffer;
        if (buffer->project == project)
        {
            DestroyBuffer(buffer->id);
        }
    }
    Assert(project->associated_buffer_count == 0);

    project->root = "FREE PROJECT"_str;

    DllRemove(project);
    SllStackPush(project, editor->first_free_project);
}

function void
AssociateProject(Buffer *buffer)
{
    if (buffer->project) return;

    Project *project = nullptr;
    for (ProjectIterator it = IterateProjects(); IsValid(&it); Next(&it))
    {
        if (MatchPrefix(buffer->full_path, it.project->root, StringMatch_CaseInsensitive))
        {
            project = it.project;
            break;
        }
    }

    if (!project)
    {
        project = CreateProject(buffer->full_path);
    }

    Tags *tags = buffer->tags;
    for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
    {
        uint32_t slot = tag->hash.u32[0] % ArrayCount(project->tag_table);
        tag->next_in_hash = project->tag_table[slot];
        project->tag_table[slot] = tag;
    }

    buffer->project = project;
    project->associated_buffer_count += 1;
}

function void
RemoveProjectAssociation(Buffer *buffer)
{
    Project *project = buffer->project;
    project->associated_buffer_count -= 1;
    project = nullptr;
}

//
// ProjectIterator
//

function ProjectIterator
IterateProjects()
{
    ProjectIterator it = {};
    it.project = editor->project_sentinel.next;
    return it;
}

function bool
IsValid(ProjectIterator *it)
{
    return it->project != &editor->project_sentinel;
}

function void
Next(ProjectIterator *it)
{
    it->project = it->project->next;
}

function void
DestroyCurrent(ProjectIterator *it)
{
    Project *project = it->project;
    it->project = it->project->next;
    DestroyProject(project);
}
