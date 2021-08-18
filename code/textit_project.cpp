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
OpenCodeFilesRecursively(String search_start)
{
    ScopedMemory temp;

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
            OpenCodeFilesRecursively(PushTempStringF("%.*s%.*s\\", StringExpand(search_path), StringExpand(it->info.name)));
        }
        else
        {
            String ext;
            SplitExtension(it->info.name, &ext);

            if (GetLanguageFromExtension(ext))
            {
                OpenBufferFromFile(PushTempStringF("%.*s%.*s", StringExpand(search_path), StringExpand(it->info.name)));
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
MakeNewProject(String search_start)
{
    Project *project = PushStruct(&editor->transient_arena, Project);
    project->root = FindProjectRoot(&editor->transient_arena, search_start);

    project->next = editor->first_project;
    editor->first_project = project;

    OpenCodeFilesRecursively(project->root);

    return project;
}

function void
AssociateProject(Buffer *buffer)
{
    if (buffer->project) return;

    Project *project = nullptr;
    for (Project *test_proj = editor->first_project; test_proj; test_proj = test_proj->next)
    {
        if (MatchPrefix(buffer->full_path, test_proj->root, StringMatch_CaseInsensitive))
        {
            project = test_proj;
        }
    }

    if (!project)
    {
        project = MakeNewProject(buffer->full_path);
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
    buffer->project->associated_buffer_count -= 1;
    // TODO: Handle projects having 0 bufferss
    buffer->project = nullptr;
}