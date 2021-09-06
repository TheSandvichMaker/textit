COMMAND_PROC(ReportMetrics,
             "Report some metrics relevant to development"_str)
{
    Buffer *buffer = GetActiveBuffer();
    size_t buffer_bytes = buffer->count;

    Project *project = buffer->project;

    LineIndexCountResult index_stats = {};
    CountLineIndex(buffer->line_index_root, &index_stats);

    size_t buffer_tag_count = 0;
    size_t buffer_tag_bytes = 0;
    {
        Tags *tags = buffer->tags;
        for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
        {
            buffer_tag_count += 1;
            buffer_tag_bytes += sizeof(*tag);
        }
    }

    platform->DebugPrint("Memory usage for buffer %.*s\n"
                         "\tBuffer Text:    %s/%s\n"
                         "\tBuffer Arena:   %s/%s\n"
                         "\tLine Index:     %s\n"
                         "\tToken Blocks:   %s (occupied: %zu/%zu (%.02f%%%%))\n"
                         "\tTags:           %s (%zu tags)\n",
                         StringExpand(buffer->name),
                         FormatHumanReadableBytes(buffer_bytes).data,
                         FormatHumanReadableBytes(TEXTIT_BUFFER_SIZE).data,
                         FormatHumanReadableBytes(buffer->arena.used).data,
                         FormatHumanReadableBytes(buffer->arena.capacity).data,
                         FormatHumanReadableBytes(index_stats.nodes_size).data,
                         FormatHumanReadableBytes(index_stats.token_blocks_size).data,
                         index_stats.token_blocks_occupancy,
                         index_stats.token_blocks_capacity,
                         100.0*((double)index_stats.token_blocks_occupancy / (double)index_stats.token_blocks_capacity),
                         FormatHumanReadableBytes(buffer_tag_bytes).data, 
                         buffer_tag_count);

    size_t project_tag_count = 0;
    size_t project_tag_bytes = 0;
    for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
    {
        if (it.buffer->project != project) continue;

        Tags *tags = it.buffer->tags;
        for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
        {
            project_tag_count += 1;
            project_tag_bytes += sizeof(*tag);
        }
    }

    platform->DebugPrint("Memory usage for project %.*s\n"
                         "\tProject struct: %s\n"
                         "\tTags:           %s (%zu tags)\n",
                         StringExpand(project->root),
                         FormatHumanReadableBytes(sizeof(*project)).data,
                         FormatHumanReadableBytes(project_tag_bytes).data,
                         project_tag_count);
}

COMMAND_PROC(ResetGlyphCache,
             "Reset the glyph cache"_str)
{
    ClearBitmap(&render_state->glyph_texture.bitmap);
    uint32_t size = render_state->glyph_cache.size;
    ResizeGlyphCache(&render_state->glyph_cache, size);
}

COMMAND_PROC(ValidateLineIndex,
             "Run validation tests on the line index of the current buffer"_str)
{
    ValidateLineIndexFull(GetActiveBuffer());
}

COMMAND_PROC(LoadDefaultIndentRules,
             "Load the default indent rules for the current buffer"_str)
{
    LoadDefaultIndentRules(&editor->default_indent_rules);
}

COMMAND_PROC(LoadOtherIndentRules,
             "Load the other indent rules for the current buffer"_str)
{
    LoadOtherIndentRules(&editor->default_indent_rules);
}

COMMAND_PROC(EnterCommandLineMode)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Command"_str;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (size_t i = 0; i < command_list->command_count; i += 1)
        {
            Command *command = &command_list->commands[i];
            if ((command->kind == Command_Basic) &&
                (command->flags & Command_Visible) &&
                (FindSubstring(command->name, string, StringMatch_CaseInsensitive) != command->name.size))
            {
                if (!AddPrediction(cl, MakePrediction(command->name)))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        Command *command = FindCommand(string, StringMatch_CaseInsensitive);
        if (command &&
            (command->kind == Command_Basic) &&
            (command->flags & Command_Visible))
        {
            command->command();
            return true;
        }
        return true;
    };
}

COMMAND_PROC(OpenBuffer,
             "Interactively open a buffer"_str,
             Command_Jump)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Open Buffer"_str;
    cl->no_quickselect = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        Project *active_project = GetActiveBuffer()->project;

        bool show_hidden = false;
        if (string[0] == '!')
        {
            show_hidden = true;
            string = Advance(string);
        }

        String leaf;
        String path = SplitPath(string, &leaf);

        String ext;
        String name = SplitExtension(leaf, &ext);

        // FindSubstring implementation is limited to patterns smaller than bit count of size_t
        // so for now I'll just do this
        size_t size_bits = 8*sizeof(size_t);
        if (ext.size  >= size_bits) ext.size  = size_bits - 1;
        if (name.size >= size_bits) name.size = size_bits - 1;

        for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
        {
            Buffer *buffer = it.buffer;
            if (!show_hidden && buffer->project != active_project) continue;

            String test_ext = {};
            String test_name = buffer->name;
            if (ext.size)
            {
                test_name = SplitExtension(test_name, &test_ext);
            }

            if (string.size == 0 ||
                (FindSubstring(test_name, name, StringMatch_CaseInsensitive) != test_name.size &&
                 (!ext.size || FindSubstring(test_ext, ext, StringMatch_CaseInsensitive) != test_ext.size)))
            {
                bool non_standard_language = (buffer->language != buffer->inferred_language);

                Prediction prediction = {};
                prediction.text = buffer->name;
                // TODO: Undumb
                if (buffer->project != active_project)
                {
                    if (non_standard_language)
                    {
                        prediction.preview_text = PushTempStringF("%-32.*s (language: %.*s) - (%.*s)", 
                                                                  StringExpand(buffer->name), 
                                                                  StringExpand(buffer->language->name), 
                                                                  StringExpand(buffer->project->root));
                    }
                    else
                    {
                        prediction.preview_text = PushTempStringF("%-32.*s - (%.*s)", 
                                                                  StringExpand(buffer->name), 
                                                                  StringExpand(buffer->project->root));
                    }
                    prediction.color        = "command_line_option_directory"_id;
                }
                else if (non_standard_language)
                {
                    prediction.preview_text = PushTempStringF("%.*s (language: %.*s)", 
                                                              StringExpand(buffer->name), 
                                                              StringExpand(buffer->language->name));
                }

                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
        {
            Buffer *buffer = it.buffer;
            if (AreEqual(buffer->name, string))
            {
                View *view = GetActiveView();
                view->next_buffer = buffer->id;
                return true;
            }
        }
        return true;
    };
}

COMMAND_PROC(OpenFile,
             "Interactively open a file"_str,
             Command_Jump)
{
    CommandLine *cl = BeginCommandLine();
    cl->name           = "Open File"_str;
    cl->no_quickselect = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        String leaf;
        String path = SplitPath(string, &leaf);

        String ext;
        String name = SplitExtension(leaf, &ext);

        // FindSubstring implementation is limited to patterns smaller than bit count of size_t
        // so for now I'll just do this
        size_t size_bits = 8*sizeof(size_t);
        if (ext.size  >= size_bits) ext.size  = size_bits - 1;
        if (name.size >= size_bits) name.size = size_bits - 1;

        char *separator = "/";
        if (PeekEnd(path) == '\\') separator = "\\";

        for (PlatformFileIterator *it = platform->FindFiles(platform->GetTempArena(), path);
             platform->FileIteratorIsValid(it);
             platform->FileIteratorNext(it))
        {
            if (AreEqual(it->info.name, "."_str) ||
                AreEqual(it->info.name, ".."_str))
            {
                continue;
            }

            String test_ext = {};
            String test_name = it->info.name;
            if (ext.size)
            {
                test_name = SplitExtension(test_name, &test_ext);
            }

            if (FindSubstring(test_name, name, StringMatch_CaseInsensitive) != test_name.size &&
                (!ext.size || FindSubstring(test_ext, ext, StringMatch_CaseInsensitive) != test_ext.size))
            {
                Prediction prediction = {};
                prediction.text         = PushTempStringF("%.*s%.*s%s", StringExpand(path), StringExpand(it->info.name), it->info.directory ? separator : "");
                prediction.preview_text = PushTempStringF("%.*s%s", StringExpand(it->info.name), it->info.directory ? separator : "");
                if (it->info.directory)
                {
                    prediction.incomplete = true;
                    prediction.color      = "command_line_option_directory"_id;
                }
                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        View *view = GetActiveView();
        for (BufferIterator it = IterateBuffers(); IsValid(&it); Next(&it))
        {
            Buffer *buffer = it.buffer;
            if (AreEqual(buffer->name, string))
            {
                view->next_buffer = buffer->id;
                return true;
            }
        }

        view->next_buffer = OpenBufferFromFile(string)->id;
        return true;
    };
}

COMMAND_PROC(ToggleProjectVisibility,
             "Toggle the visibility of projects"_str,
             Command_Jump)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Toggle Project Visibility"_str;
    cl->no_quickselect = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (Project *project = editor->first_project; project; project = project->next)
        {
            if (string.size == 0 ||
                (FindSubstring(project->root, string, StringMatch_CaseInsensitive) != project->root.size))
            {
                Prediction prediction = {};
                prediction.text = project->root;
                if (HasFlag(project->flags, Project_Hidden))
                {
                    prediction.preview_text = PushTempStringF("(hidden)  %.*s", StringExpand(project->root));
                    prediction.color        = "command_line_option_inactive"_id;
                }
                else
                {
                    prediction.preview_text = PushTempStringF("(visible) %.*s", StringExpand(project->root));
                    prediction.color        = "command_line_option_active"_id;
                }

                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (Project *project = editor->first_project; project; project = project->next)
        {
            if (AreEqual(project->root, string))
            {
                if (HasFlag(project->flags, Project_Hidden))
                {
                    project->flags &= ~Project_Hidden;
                }
                else
                {
                    project->flags |= Project_Hidden;
                }
                return true;
            }
        }
        return true;
    };
}

COMMAND_PROC(ChangeWorkingDirectory,
             "Change the working directory"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name           = "Change Working Directory"_str;
    cl->no_quickselect = true;
    cl->no_autoaccept  = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        String path = SplitPath(string);

        char *separator = "/";
        if (PeekEnd(path) == '\\') separator = "\\";

        for (PlatformFileIterator *it = platform->FindFiles(platform->GetTempArena(), string);
             platform->FileIteratorIsValid(it);
             platform->FileIteratorNext(it))
        {
            if (AreEqual(it->info.name, "."_str) ||
                AreEqual(it->info.name, ".."_str))
            {
                continue;
            }

            if (it->info.directory)
            {
                Prediction prediction = {};
                prediction.text         = PushTempStringF("%.*s%.*s%s", StringExpand(path), StringExpand(it->info.name), separator);
                prediction.preview_text = it->info.name;
                prediction.color        = "command_line_option_directory"_id;
                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        platform->SetWorkingDirectory(string);
        return true;
    };
}

COMMAND_PROC(Set,
             "Set a config value"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Set"_str;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (size_t i = 0; i < Introspection<CoreConfig>::member_count; i += 1)
        {
            const MemberInfo *member = &Introspection<CoreConfig>::members[i];

            String name = SplitWord(member->name);
            if (FindSubstring(name, string, StringMatch_CaseInsensitive) != name.size)
            {
                Prediction prediction = {};
                prediction.text = name;

                String formatted_value = FormatMember(member, GetMemberPointer(core_config, member));
                prediction.preview_text = PushTempStringF("%-32.*s %.*s", StringExpand(name), StringExpand(formatted_value));
                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        cl = BeginCommandLine();
        cl->name = string;
        cl->AcceptEntry = [](CommandLine *cl)
        {
            String string = GetCommandString(cl);

            for (size_t i = 0; i < Introspection<CoreConfig>::member_count; i += 1)
            {
                const MemberInfo *member = &Introspection<CoreConfig>::members[i];

                String short_name = SplitWord(member->name);
                if (AreEqual(short_name, cl->name))
                {
                    return ParseAndWriteMember(core_config, member, string);
                }
            }

            return true;
        };

        return true;
    };
}

COMMAND_PROC(SetLanguage,
             "Set the language for the active buffer"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Set"_str;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (LanguageSpec *language = language_registry->first_language;
             language;
             language = language->next)
        {
            if (FindSubstring(language->name, string, StringMatch_CaseInsensitive) != language->name.size)
            {
                Prediction prediction = {};
                prediction.text = language->name;
                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (LanguageSpec *language = language_registry->first_language;
             language;
             language = language->next)
        {
            if (AreEqual(language->name, string, StringMatch_CaseInsensitive))
            {
                Buffer *buffer = GetActiveBuffer();
                buffer->language = language;
                TokenizeBuffer(buffer);
                ParseTags(buffer);
                return true;
            }
        };

        return true;
    };
}

COMMAND_PROC(SetFont,
             "Set the font for the editor"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "SetFont"_str;
    cl->no_quickselect = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        ScopedMemory temp;

        uint32_t font_count;
        String *filtered_fonts = platform->EnumerateFonts(temp, ArrayCount(cl->predictions) + 1, string, &font_count);

        for (size_t i = 0; i < font_count; i += 1)
        {
            Prediction prediction = {};
            prediction.text = filtered_fonts[i];
            if (!AddPrediction(cl, prediction))
            {
                break;
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        SetEditorFont(string, editor->font_size, editor->font_quality);
        return true;
    };
}

COMMAND_PROC(SetFontQuality,
             "Set the quality of the font for the editor"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Font Quality"_str;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        String options[] =
        {
            "Subpixel"_str,
            "Greyscale"_str,
            "Raster"_str,
        };
        for (size_t i = 0; i < ArrayCount(options); i += 1)
        {
            if (FindSubstring(options[i], string, StringMatch_CaseInsensitive) != options[i].size)
            {
                AddPrediction(cl, MakePrediction(options[i]));
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        PlatformFontQuality quality = PlatformFontQuality_SubpixelAA;
        if      (AreEqual(string, "Subpixel"_str))  quality = PlatformFontQuality_SubpixelAA;
        else if (AreEqual(string, "Greyscale"_str)) quality = PlatformFontQuality_GreyscaleAA;
        else if (AreEqual(string, "Raster"_str))    quality = PlatformFontQuality_Raster;
        else
        {
            return false;
        }

        SetEditorFont(editor->font_name.as_string, editor->font_size, quality);
        return true;
    };
}

COMMAND_PROC(SetFontSize,
             "Set the size of the font for the editor"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Font Size"_str;

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        int64_t size;
        if (ParseInt(string, nullptr, &size))
        {
            size = Clamp(size, 4, 128);
            SetEditorFont(editor->font_name.as_string, (int)size, editor->font_quality);
            return true;
        }
        return false;
    };
}

COMMAND_PROC(GoToFileUnderCursor,
             "Go to the file under the cursor, if there is one"_str,
             Command_Jump)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Token token = GetTokenAt(buffer, cursor->pos, GetToken_EncloseStrings);
    if (token.kind == Token_String)
    {
        ScopedMemory temp;
        String string = PushBufferRange(temp, buffer, GetInnerRange(token));

        for (PlatformFileIterator *it = platform->FindFiles(temp, string);
             platform->FileIteratorIsValid(it);
             platform->FileIteratorNext(it))
        {
            if (AreEqual(it->info.name, string, StringMatch_CaseInsensitive))
            {
                view->next_buffer = OpenBufferFromFile(string)->id;
                break;
            }
        }
    }
}

COMMAND_PROC(Tags,
             "Browse tags for all buffers"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name                  = "Tags"_str;
    cl->sort_by_edit_distance = true;
    cl->no_quickselect        = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        // Project *active_project = GetActiveBuffer()->project;

        for (BufferIterator it = IterateBuffers();
             IsValid(&it);
             Next(&it))
        {
            Buffer *buffer = it.buffer;
            // TODO: What should da policy be for showing tags from other projects?
            // if (buffer->project != active_project) continue;

            Tags *tags = buffer->tags;
            for (Tag *tag = tags->sentinel.next; tag != &tags->sentinel; tag = tag->next)
            {
                ScopedMemory temp;
                String name = PushBufferRange(temp, buffer, MakeRangeStartLength(tag->pos, tag->length));
                if (FindSubstring(name, string, StringMatch_CaseInsensitive) != name.size)
                {
                    int64_t line = GetLineNumber(buffer, tag->pos);
                    String kind_name     = GetTagBaseKindName(tag);
                    String sub_kind_name = GetTagSubKindName(buffer->language, tag);
                    Prediction prediction = {};
                    prediction.text         = name;
                    prediction.preview_text = PushTempStringF("%-48.*s %.*s %.*s -- %.*s (line: %lld)", 
                                                              StringExpand(name), 
                                                              StringExpand(sub_kind_name), 
                                                              StringExpand(kind_name), 
                                                              StringExpand(buffer->name),
                                                              line);
                    if (!AddPrediction(cl, prediction))
                    {
                        break;
                    }
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        View *view = GetActiveView();
        Buffer *buffer = GetBuffer(view);

        // TODO: How do we know which tag you really wanted?
        ScopedMemory temp;
        if (Tag *tag = PushTagsWithName(temp, buffer->project, string))
        {
            JumpToLocation(view, tag->buffer, tag->pos);
            view->center_view_next_time_we_calculate_scroll = true; // this is terrible
        }

        return true;
    };
}

COMMAND_PROC(SetTheme,
             "Set the theme for the editor"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "SetTheme"_str;
    cl->no_quickselect = true;

    cl->GatherPredictions = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);

        for (Theme *theme = editor->first_theme; theme; theme = theme->next)
        {
            if (FindSubstring(theme->name, string, StringMatch_CaseInsensitive) != theme->name.size)
            {
                Prediction prediction = {};
                prediction.text = theme->name;
                if (!AddPrediction(cl, prediction))
                {
                    break;
                }
            }
        }
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        String string = GetCommandString(cl);
        SetEditorTheme(string);
        return true;
    };
}

function void
FindCharInternal(Direction direction)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Find Char"_str;
    if (direction == Direction_Backward)
    {
        cl->name = "Find Char (Backward)"_str;
    }

    cl->userdata = IntToPointer(direction);

    cl->OnText = [](CommandLine *cl, String text)
    {
        View   *view   = GetActiveView();
        Buffer *buffer = GetBuffer(view);

        Direction direction = (Direction)PointerToInt(cl->userdata);

        uint8_t c = text[0];
        if (!c) return text;

        for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
        {
            int64_t start = cursor->pos;
            int64_t pos   = start + direction;
            while (ReadBufferByte(buffer, pos) != c)
            {
                pos += direction;
            }
            if (pos < buffer->count)
            {
                SetCursor(cursor, pos, MakeRange(start, pos));
            }
        }

        Terminate(cl);
        return text;
    };
}

function void
ToCharInternal(Direction direction)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "To Char"_str;
    if (direction == Direction_Backward)
    {
        cl->name = "To Char (Backward)"_str;
    }

    cl->userdata = IntToPointer(direction);

    cl->OnText = [](CommandLine *cl, String text)
    {
        View   *view   = GetActiveView();
        Buffer *buffer = GetBuffer(view);

        Direction direction = (Direction)PointerToInt(cl->userdata);

        uint8_t c = text[0];
        if (!c) return text;

        for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
        {
            int64_t start = cursor->pos;
            int64_t pos   = start + direction;
            while (ReadBufferByte(buffer, pos + direction) != c)
            {
                pos += direction;
            }
            if (pos < buffer->count)
            {
                SetCursor(cursor, pos, MakeRange(start, pos));
            }
        }

        Terminate(cl);
        return text;
    };
}

COMMAND_PROC(FindChar,
             "Find the next occurrence of a character from the cursor"_str)
{
    FindCharInternal(Direction_Forward);
}

COMMAND_PROC(FindCharBackward,
             "Find the previous occurrence of a character from the cursor"_str)
{
    FindCharInternal(Direction_Backward);
}

COMMAND_PROC(ToChar,
             "Seek to the next occurrence of a character from the cursor"_str)
{
    ToCharInternal(Direction_Forward);
}

COMMAND_PROC(ToCharBackward,
             "Seek to the previous occurrence of a character from the cursor"_str)
{
    ToCharInternal(Direction_Backward);
}

COMMAND_PROC(Search,
             "Do a text search in the current buffer"_str)
{
    CommandLine *cl = BeginCommandLine();
    cl->name = "Search"_str;

    View *view = GetActiveView();

    // save a copy of the current cursors so we can revert to them
    Cursor *first_cursor = nullptr, *last_cursor = nullptr;
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        Cursor *backup_cursor = PushStruct(cl->arena, Cursor);
        CopyStruct(cursor, backup_cursor);
        SllQueuePush(first_cursor, last_cursor, backup_cursor);
    }

    struct SearchData
    {
        String original_search;
        Cursor *backup_cursors;
        int64_t scroll;
    };

    SearchData *data = PushStruct(cl->arena, SearchData);
    data->original_search = PushString(cl->arena, editor->search.as_string);
    data->backup_cursors  = first_cursor;
    data->scroll          = view->scroll_at;
    cl->userdata = data;

    cl->OnText = [](CommandLine *cl, String text)
    {
        View   *view   = GetActiveView();
        Buffer *buffer = GetBuffer(view);

        SearchData *data = (SearchData *)cl->userdata;
        Cursor *backup_cursors = data->backup_cursors;
        view->scroll_at = data->scroll;

        String search = GetCommandString(cl);
        Replace(&editor->search, search);

        editor->show_search_highlight = true;
        editor->search_flags = StringMatch_CaseInsensitive;

        for (Cursor *cursor = IterateCursors(view), *backup_cursor = backup_cursors; 
             cursor; 
             cursor = cursor->next, backup_cursor = backup_cursor->next)
        {
            cursor->pos        = backup_cursor->pos;
            cursor->selection  = backup_cursor->selection;
            cursor->sticky_col = backup_cursor->sticky_col;

            Range range = FindNextOccurrence(buffer, cursor->pos, search, editor->search_flags);
            SetCursor(cursor, range.start, range);
        }

        return text;
    };

    cl->AcceptEntry = [](CommandLine *cl)
    {
        // I'm just saving the old location of the first cursor in jump
        // history if you accept the search.

        View *view = GetActiveView();

        SearchData *data = (SearchData *)cl->userdata;
        SaveJump(view, view->buffer, data->backup_cursors->pos);

        editor->last_movement = FindCommand("RepeatLastSearch"_str);

        return true;
    };

    cl->OnTerminate = [](CommandLine *cl)
    {
        View *view = GetActiveView();

        SearchData *data = (SearchData *)cl->userdata;
        Cursor *backup_cursors = data->backup_cursors;
        view->scroll_at = data->scroll;

        Replace(&editor->search, data->original_search);

        for (Cursor *cursor = IterateCursors(view), *backup_cursor = backup_cursors; 
             cursor; 
             cursor = cursor->next, backup_cursor = backup_cursor->next)
        {
            cursor->pos        = backup_cursor->pos;
            cursor->selection  = backup_cursor->selection;
            cursor->sticky_col = backup_cursor->sticky_col;
        }
    };
}

MOVEMENT_PROC(RepeatLastSearch, Movement_NoAutoRepeat)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    editor->show_search_highlight = true;
    String search = editor->search.as_string;

    Range range = FindNextOccurrence(buffer, cursor->pos + 1, search, editor->search_flags);

    Move move = {};
    move.pos = range.start;
    move.selection.inner = range;
    move.selection.outer = range;
    return move;
}

MOVEMENT_PROC(RepeatLastSearchBackward, Movement_NoAutoRepeat)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    editor->show_search_highlight = true;
    String search = editor->search.as_string;

    Range range = FindPreviousOccurrence(buffer, cursor->pos - 1, search, editor->search_flags);

    Move move = {};
    move.pos = range.start;
    move.selection.inner = range;
    move.selection.outer = range;
    return move;
}

COMMAND_PROC(GoToDefinitionUnderCursor,
             "Go to the definition of the token under the cursor (type, function, file, etc)"_str,
             Command_Jump)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Token token = GetTokenAt(buffer, cursor->pos, GetToken_EncloseStrings);
    if (token.kind == Token_Identifier ||
        token.kind == Token_Function)
    {
        ScopedMemory temp;
        String string = PushBufferRange(temp, buffer, MakeRangeStartLength(token.pos, token.length));

        if (Tag *tags = PushTagsWithName(temp, buffer->project, string))
        {
            Tag *best_tag = tags;
            for (Tag *tag = tags; tag; tag = tag->next)
            {
                if ((tag->related_token_kind == token.kind) &&
                    (tag->kind > best_tag->kind))
                {
                    best_tag = tag;
                }
            }
            JumpToLocation(view, best_tag->buffer, best_tag->pos);
            view->center_view_next_time_we_calculate_scroll = true; // this is terrible
        }
    }
    else if (token.kind == Token_String)
    {
        ScopedMemory temp;
        String string = PushBufferRange(temp, buffer, MakeRangeStartLength(token.pos + 1, token.length - 2));

        if (Buffer *found = FindOrOpenBuffer(buffer->project, string))
        {
            view->next_buffer = found->id;
        }
    }
}

function void
CreateNewCursorOnNextLine(View *view, Direction direction)
{
    Buffer *buffer = GetBuffer(view);

    int64_t delta = direction;

    Cursor *relevant_cursor = nullptr;
    for (Cursor *cursor = IterateCursors(view);
         cursor;
         cursor = cursor->next)
    {
        if (!relevant_cursor) relevant_cursor = cursor;
        if (direction == Direction_Forward  && relevant_cursor->pos < cursor->pos) relevant_cursor = cursor;
        if (direction == Direction_Backward && relevant_cursor->pos > cursor->pos) relevant_cursor = cursor;
    }

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, relevant_cursor->pos);
    int64_t line = loc.line;
    int64_t col  = loc.col;

    {
        Range inner, outer;
        GetLineRanges(buffer, line, &inner, &outer);

        SetCursor(relevant_cursor, relevant_cursor->pos, inner, outer);
    }

    for (;;)
    {
        line += delta;

        if (!LineIsInBuffer(buffer, line))
        {
            break;
        }

        Range range = GetLineRange(buffer, line);

        ScopedMemory temp;
        String line_string = PushBufferRange(temp, buffer, range);

        bool line_empty = true;
        for (size_t i = 0; i < line_string.size; i += 1)
        {
            if (!IsWhitespaceAscii(line_string.data[i]))
            {
                line_empty = false;
            }
        }

        if (!line_empty)
        {
            Cursor *new_cursor = CreateCursor(view);
            new_cursor->sticky_col = relevant_cursor->sticky_col;

            Range inner, outer;
            GetLineRanges(buffer, line, &inner, &outer);

            SetCursor(new_cursor, CalculateBufferLocationFromLineCol(buffer, line, col).pos, inner, outer);
            break;
        }
    }
}

COMMAND_PROC(NewCursorUp)
{
    View *view = GetActiveView();
    CreateNewCursorOnNextLine(view, Direction_Backward);
}

COMMAND_PROC(NewCursorDown)
{
    View *view = GetActiveView();
    CreateNewCursorOnNextLine(view, Direction_Forward);
}

COMMAND_PROC(NextJump)
{
    View *view = GetActiveView();
    if (Jump *jump = NextJump(view))
    {
        JumpToLocation(view, jump->buffer, jump->pos);
    }
}

COMMAND_PROC(PreviousJump)
{
    View *view = GetActiveView();
    if (view->jump_at + 1 > view->jump_top)
    {
        SaveJump(view, view->buffer, GetCursor(view)->pos);
        PreviousJump(view);
    }
    Jump jump = PreviousJump(view);
    JumpToLocation(view, jump.buffer, jump.pos);
}

COMMAND_PROC(ResetCursors)
{
    editor->show_search_highlight = false;

    View *view = GetActiveView();
    Cursor **slot = GetCursorSlot(view->id, view->buffer, true);
    for (Cursor *cursor = *slot; cursor->next; cursor = *slot)
    {
        cursor->selection.inner = MakeRange(cursor->pos);
        cursor->selection.outer = MakeRange(cursor->pos);
        *slot = cursor->next;
        FreeCursor(cursor);
    }
    Cursor *cursor = *slot;
    cursor->selection.inner = MakeRange(cursor->pos);
    cursor->selection.outer = MakeRange(cursor->pos);
}

COMMAND_PROC(Exit, "Exit the editor"_str)
{
    platform->exit_requested = true;
}

COMMAND_PROC(qa, "Exit the editor"_str)
{
    platform->exit_requested = true;
}

COMMAND_PROC(ToggleVisualizeNewlines, "Toggle the visualization of newlines"_str)
{
    core_config->visualize_newlines = !core_config->visualize_newlines;
}

COMMAND_PROC(ToggleVisualizeWhitespace, "Toggle the visualization of whitespaces"_str)
{
    core_config->visualize_whitespace = !core_config->visualize_whitespace;
}

COMMAND_PROC(ToggleLineNumbers, "Toggle the line number display"_str)
{
    core_config->show_line_numbers = !core_config->show_line_numbers;
}

COMMAND_PROC(EnterTextMode,
             "Enter Text Input Mode"_str)
{
    editor->next_edit_mode = EditMode_Text;
    BeginUndoBatch(GetActiveBuffer());
}

COMMAND_PROC(Append)
{
    View *view = GetActiveView();
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        cursor->pos += 1;
    }
    CMD_EnterTextMode();
}

COMMAND_PROC(AppendAtEnd)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        FindLineEnd(buffer, cursor->pos, &cursor->pos);
    }
    CMD_EnterTextMode();
}

COMMAND_PROC(EnterCommandMode,
             "Enter Command Mode"_str)
{
    editor->next_edit_mode = EditMode_Command;

    Buffer *buffer = GetActiveBuffer();
    EndUndoBatch(buffer);

    View *view = GetActiveView();
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        BufferLocation loc = CalculateRelativeMove(buffer, cursor, MakeV2i(-1, 0));
        cursor->pos = loc.pos;
        cursor->selection = MakeSelection(cursor->pos);
        cursor->sticky_col = loc.col;
    }
}

COMMAND_PROC(CenterView,
             "Center the view around the cursor"_str)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);
    BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);

    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    view->scroll_at = loc.line - viewport_height / 2;
}

COMMAND_PROC(JumpToBufferStart, "Jump to the start of the buffer"_str)
{
    View *view = GetActiveView();
    SetCursor(view, 0);
}

COMMAND_PROC(JumpToBufferEnd, "Jump to the end of the buffer"_str)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    SetCursor(view, buffer->count - 1);
}

MOVEMENT_PROC(MoveLeft)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos - 1, line_range);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);
    cursor->sticky_col = loc.col;

    return MakeMove(pos);
}

MOVEMENT_PROC(MoveRight)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos);
    pos = ClampToRange(pos + 1, line_range);

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, pos);
    cursor->sticky_col = loc.col;

    return MakeMove(pos);
}

MOVEMENT_PROC(MoveLeftIdentifier)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Selection selection = ScanWordBackward(buffer, pos);

    Move result = {};
    result.pos = selection.outer.end;
    result.selection = selection;

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.pos);
    cursor->sticky_col = loc.col;

    return result;
}

MOVEMENT_PROC(MoveRightIdentifier)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Selection selection = ScanWordForward(buffer, pos);

    Move result = {};
    result.pos = selection.outer.end;
    result.selection = selection;

    BufferLocation loc = CalculateBufferLocationFromPos(buffer, result.pos);
    cursor->sticky_col = loc.col;

    return result;
}

MOVEMENT_PROC(MoveLineStart)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 0;

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(MakeRange(pos, line_range.start));
}

MOVEMENT_PROC(MoveLineEnd)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->sticky_col = 9999;

    int64_t pos = cursor->pos;

    int64_t inner, outer;
    FindLineEnd(buffer, pos, &inner, &outer);

    Move move = {};
    move.selection.inner.start = pos;
    move.selection.outer.start = pos;
    move.selection.inner.end   = inner;
    move.selection.outer.end   = outer;
    move.pos                   = inner;

    return move;
}

MOVEMENT_PROC(EncloseLine)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range line_range = EncloseLine(buffer, pos, true);
    return MakeMove(line_range);
}

MOVEMENT_PROC(EncloseNextScope)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    Range result = MakeRange(pos);

    TokenIterator it = IterateTokens(buffer, pos);

    TokenKind opening_kind = Token_None;
    TokenKind closing_kind = Token_None;
    bool seek_forward = true;
    int depth = 0;
    while (IsValid(&it))
    {
        Token t = Next(&it);
        if ((t.kind == Token_LeftParen) ||
            (t.kind == Token_LeftScope) ||
            (t.kind == Token_RightParen) ||
            (t.kind == Token_RightScope))
        {
            opening_kind = t.kind;
            if (opening_kind == Token_LeftParen)  { seek_forward = true;  closing_kind = Token_RightParen; }
            if (opening_kind == Token_LeftScope)  { seek_forward = true;  closing_kind = Token_RightScope; }
            if (opening_kind == Token_RightParen) { seek_forward = false; closing_kind = Token_LeftParen; Prev(&it); }
            if (opening_kind == Token_RightScope) { seek_forward = false; closing_kind = Token_LeftScope; Prev(&it); }

            depth = 1;
            break;
        }
    }
    if (opening_kind)
    {
        while (IsValid(&it))
        {
            Token t = (seek_forward ? Next(&it) : Prev(&it));
            if (t.kind == opening_kind)
            {
                depth += 1;
            }
            else if (t.kind == closing_kind)
            {
                depth -= 1;
                if (depth <= 0)
                {
                    result.end = (seek_forward ? t.pos + t.length : t.pos);
                    break;
                }
            }
        }
    }

    return MakeMove(result);
}

function Move
SelectSurroundingNest(View *view,
                      TokenKind open_nest, TokenKind close_nest,
                      bool line_selection)
{
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move result = {};
    result.pos       = cursor->pos;
    result.selection = cursor->selection;
    
    int64_t pos = cursor->pos;

    int64_t start_pos       = -1;
    int64_t inner_start_pos = -1;

    TokenIterator it = IterateTokens(buffer, pos);

    int depth = 0;
    while (IsValid(&it))
    {
        Token t = Prev(&it);
        if (!t.kind) break;

        if (t.kind == close_nest)
        {
            depth += 1;
        }
        if (t.kind == open_nest)
        {
            depth -= 1;
            if (depth < 0)
            {
                start_pos = t.pos;
                inner_start_pos = t.pos + t.length;
                break;
            }
        }
    }

    depth = 0;
    if (start_pos >= 0)
    {
        while (IsValid(&it))
        {
            Token t = Next(&it);
            if (!t.kind) break;

            if (t.kind == open_nest)
            {
                depth += 1;
            }
            else if (t.kind == close_nest)
            {
                depth -= 1;
                if (depth < 0)
                {
                    int64_t end_pos       = t.pos + t.length;
                    int64_t inner_end_pos = t.pos;

                    result.selection.inner.start = inner_start_pos;
                    result.selection.inner.end   = inner_end_pos;
                    result.selection.outer.start = start_pos;
                    result.selection.outer.end   = end_pos;

                    if (line_selection)
                    {
                        int64_t start_line = GetLineNumber(buffer, result.selection.outer.start);
                        int64_t end_line   = GetLineNumber(buffer, result.selection.outer.end);

                        int64_t inner_start_line = start_line + 1;
                        int64_t inner_end_line   = Max(start_line, end_line - 1);

                        if (start_line != end_line)
                        {
                            result.selection.outer.start = GetLineRange(buffer, start_line).start;
                            result.selection.outer.end   = GetLineRange(buffer, end_line).end;

                            result.selection.inner.start = GetInnerLineRange(buffer, inner_start_line).start;
                            result.selection.inner.end   = GetInnerLineRange(buffer, inner_end_line).end;
                        }

                    }

                    result.pos = result.selection.outer.start;

                    break;
                }
            }
        }
    }

    return result;
}

MOVEMENT_PROC(EncloseSurroundingScope, Movement_NoAutoRepeat)
{
    View *view = GetActiveView();
    return SelectSurroundingNest(view, Token_LeftScope, Token_RightScope, true);
}

MOVEMENT_PROC(EncloseSurroundingParen, Movement_NoAutoRepeat)
{
    View *view = GetActiveView();
    return SelectSurroundingNest(view, Token_LeftParen, Token_RightParen, false);
}

MOVEMENT_PROC(EncloseParameter)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Move result = {};
    result.pos             = cursor->pos;
    result.selection.inner = cursor->selection.inner;
    result.selection.outer = cursor->selection.outer;
    
    int64_t pos = cursor->pos;

    TokenLocator alternative_start = {};
    TokenKind end_kind      = 0;
    int64_t   end_pos       = -1;
    int64_t   inner_end_pos = -1;

    NestHelper nests = {};
    TokenIterator it = IterateTokens(buffer, pos);

    TokenKind open_token = 0;

    while (IsValid(&it) || IsValid(alternative_start))
    {
        if (!IsValid(&it) && IsValid(alternative_start))
        {
            Clear(&nests);
            Rewind(&it, alternative_start);
            alternative_start = {};
        }

        Token t = Next(&it);
        if (!t.kind) break;

        if (!IsValid(alternative_start) &&
            t.kind == Token_LeftParen)
        {
            alternative_start = LocateNext(&it);
        }

        if (IsInNest(&nests, t.kind, Direction_Forward))
        {
            continue;
        }

        if (t.kind == Token_RightParen ||
            t.kind == Token_RightScope ||
            t.kind == ';' ||
            t.kind == ',')
        {
            end_kind      = t.kind;
            inner_end_pos = t.pos;
            end_pos       = t.pos;
            if (t.kind == ',' ||
                t.kind == ';')
            {
                end_pos = t.pos + t.length;

                Token next = PeekNext(&it);
                if (next)
                {
                    end_pos = next.pos;
                }
            }
            else
            {
                open_token = GetOtherNestTokenKind(t.kind);
            }
            break;
        }
    }

    if (end_pos >= 0)
    {
        Prev(&it); // skip the end token we were on
        while (IsValid(&it))
        {
            Token t = Prev(&it);
            if (!t) break;

            if (IsInNest(&nests, t.kind, Direction_Backward))
            {
                continue;
            }

            if ((!open_token &&
                 (t.kind == Token_LeftParen   ||
                  t.kind == Token_LeftScope)) ||
                t.kind == open_token          ||
                t.kind == ',' ||
                t.kind == ';')
            {
                int64_t inner_start_pos = t.pos + t.length;

                Token next = PeekNext(&it);
                if (next)
                {
                    inner_start_pos = next.pos;
                }

                int64_t start_pos = inner_start_pos;
                if ((t.kind == ',' || t.kind == ';') && end_kind != t.kind)
                {
                    start_pos = t.pos;
                }

                result.selection.inner.start = inner_start_pos;
                result.selection.inner.end   = inner_end_pos;
                result.selection.outer.start = start_pos;
                result.selection.outer.end   = end_pos;

                result.pos = result.selection.outer.end;

                break;
            }
        }
    }

    return result;
}

MOVEMENT_PROC(MoveDown)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t line = GetLineNumber(buffer, cursor->pos);

    Move move = {};
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, 1)).pos;
    GetLineRanges(buffer, line, &move.selection.inner, &move.selection.outer);
    return move;
}

MOVEMENT_PROC(MoveUp)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t line = GetLineNumber(buffer, cursor->pos);

    Move move = {};
    move.pos = CalculateRelativeMove(buffer, cursor, MakeV2i(0, -1)).pos;
    GetLineRanges(buffer, line, &move.selection.inner, &move.selection.outer);
    return move;
}

COMMAND_PROC(PageUp)
{
    View *view = GetActiveView();
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, -Max(0, viewport_height - 4)));
}

COMMAND_PROC(PageDown)
{
    View *view = GetActiveView();
    int64_t viewport_height = view->viewport.max.y - view->viewport.min.y - 3;
    MoveCursorRelative(view, MakeV2i(0, Max(0, viewport_height - 4)));
}

COMMAND_PROC(BackspaceChar)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    for (Cursor *cursor = IterateCursors(view);
         cursor;
         cursor = cursor->next)
    {
        int64_t pos = cursor->pos;
        int64_t newline_length = PeekNewlineBackward(buffer, pos - 1);
        int64_t to_delete = 1;
        if (newline_length)
        {
            to_delete = newline_length;
        }
        else
        {
            while (IsTrailingUtf8Byte(ReadBufferByte(buffer, pos - to_delete)))
            {
                to_delete += 1;
            }

            if (ReadBufferByte(buffer, pos - 1) == ' ')
            {
                int64_t line_start = FindLineStart(buffer, pos);
                int64_t space_indent_end = line_start;
                while ((space_indent_end < pos) &&
                       (ReadBufferByte(buffer, space_indent_end) == ' '))
                {
                    space_indent_end += 1;
                }
                if (space_indent_end == pos)
                {
                    int64_t space_indent = space_indent_end - line_start;
                    if (space_indent > 0 &&
                        space_indent % core_config->indent_width == 0) // TODO: This is kind of a band-aid, really I want to actually check the auto indent to see whether this looks like an indent or alignment
                    {
                        int64_t next_indent = RoundDownNextTabStop(space_indent, core_config->indent_width);
                        to_delete = space_indent - next_indent;
                    }
                }
            }
        }

        pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos - to_delete, to_delete), ""_str);
        SetCursor(cursor, pos);
    }
}

COMMAND_PROC(BackspaceWord)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    int64_t start_pos = pos;
    int64_t end_pos = ScanWordBackward(buffer, pos).inner.end;
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursor(view, final_pos);
}

COMMAND_PROC(DeleteChar)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;
    int64_t newline_length = PeekNewline(buffer, pos);
    int64_t to_delete = 1;
    if (newline_length)
    {
        to_delete = newline_length;
    }
    pos = BufferReplaceRange(buffer, MakeRangeStartLength(pos, to_delete), ""_str);
    SetCursor(view, pos);
}

COMMAND_PROC(DeleteWord)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    int64_t pos = cursor->pos;

    int64_t start_pos = pos;
    int64_t end_pos = ScanWordEndForward(buffer, pos);
    int64_t final_pos = BufferReplaceRange(buffer, MakeRange(start_pos, end_pos), StringLiteral(""));
    SetCursor(view, final_pos);
}

COMMAND_PROC(UndoOnce)
{
    View *view = GetActiveView();

    Range result = UndoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result);
    }
}

COMMAND_PROC(RedoOnce)
{
    View *view = GetActiveView();

    Range result = RedoOnce(view);
    if (result.start >= 0)
    {
        SetCursor(view, result.start, result);
    }
}

COMMAND_PROC(SelectNextUndoBranch)
{
    SelectNextUndoBranch(GetActiveBuffer());
}

COMMAND_PROC(SplitWindowVertical, "Split the window along the vertical axis"_str)
{
    Window *window = editor->active_window;
    SplitWindow(window, WindowSplit_Vert);
}

COMMAND_PROC(SplitWindowHorizontal, "Split the window along the horizontal axis"_str)
{
    Window *window = editor->active_window;
    SplitWindow(window, WindowSplit_Horz);
}

COMMAND_PROC(DestroyWindow, "Destroy the current active window"_str)
{
    Window *window = editor->active_window;
    DestroyWindow(window);
}

COMMAND_PROC(FocusWindowLeft, "Focus the next window on the left"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Vert) &&
             (window->parent->first_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->prev;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->last_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowRight, "Focus the next window on the right"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Vert) &&
             (window->parent->last_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->next;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->first_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowDown, "Focus the next window down"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Horz) &&
             (window->parent->last_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->next;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->first_child;
        }
        editor->active_window = window;
    }
}

COMMAND_PROC(FocusWindowUp, "Focus the next window above"_str)
{
    Window *window = editor->active_window;

    for (;;)
    {
        if (!window ||
            (window->parent &&
             (window->parent->split == WindowSplit_Horz) &&
             (window->parent->first_child != window)))
        {
            break;
        }
        window = window->parent;
    }

    if (window)
    {
        window = window->prev;
        while (window->split != WindowSplit_Leaf)
        {
            window = window->last_child;
        }
        editor->active_window = window;
    }
}

CHANGE_PROC(DeleteSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    BufferReplaceRange(buffer, selection.outer, ""_str);
}

CHANGE_PROC(DeleteInnerSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    BufferReplaceRange(buffer, selection.inner, ""_str);
}

CHANGE_PROC(ChangeSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = selection.inner.start;

    Range line_range = GetLineRange(buffer, selection.inner);

    BeginUndoBatch(GetActiveBuffer());
    BufferReplaceRange(buffer, selection.inner, ""_str);
    if (line_range.start != line_range.end)
    {
        AutoIndentLineAt(buffer, cursor->pos);
    }
    CMD_EnterTextMode();
}

CHANGE_PROC(ChangeOuterSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    cursor->pos = selection.outer.start;

    Range line_range = GetLineRange(buffer, selection.outer);

    BeginUndoBatch(GetActiveBuffer());
    BufferReplaceRange(buffer, selection.outer, ""_str);
    if (line_range.start != line_range.end)
    {
        AutoIndentLineAt(buffer, cursor->pos);
    }
    CMD_EnterTextMode();
}

CHANGE_PROC(ToUppercase)
{
    Buffer *buffer = GetActiveBuffer();
    String string = PushBufferRange(platform->GetTempArena(), buffer, selection.inner);
    for (size_t i = 0; i < string.size; i += 1)
    {
        string.data[i] = ToUpperAscii(string.data[i]);
    }
    BufferReplaceRange(buffer, selection.inner, string);
}

COMMAND_PROC(RepeatLastCommand)
{
    // dummy command
}

COMMAND_PROC(Copy)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    Range range = SanitizeRange(cursor->selection.outer);

    ScopedMemory temp;
    String string = PushBufferRange(temp, buffer, range);

    platform->WriteClipboard(string);
}

COMMAND_PROC(PasteBefore)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

COMMAND_PROC(PasteAfter)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    String string = platform->ReadClipboard(platform->GetTempArena());

    int64_t insert_pos = cursor->pos + 1;
    int64_t pos = BufferReplaceRange(buffer, MakeRange(insert_pos), string);
    SetCursor(view, pos);
}

CHANGE_PROC(PasteReplaceSelection)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    Arena *arena = platform->GetTempArena();
    ScopedMemory temp_memory(arena);

    String replaced_string = PushBufferRange(arena, buffer, selection.outer);
    String string = platform->ReadClipboard(arena);

    int64_t pos = BufferReplaceRange(buffer, selection.outer, string);
    SetCursor(view, pos);

    platform->WriteClipboard(replaced_string);
}

COMMAND_PROC(UnalignCursors, "Remove excess whitespace between the cursors and their previous tokens"_str)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        uint8_t at_cursor = ReadBufferByte(buffer, cursor->pos);

        size_t space_to_leave = 1;
        if (at_cursor == ',')
        {
            space_to_leave = 0;
        }

        int64_t start = cursor->pos;
        int64_t end   = cursor->pos;
        while (start >= 0 && IsHorizontalWhitespaceAscii(ReadBufferByte(buffer, start - 1)))
        {
            start -= 1;
        }

        String space_string = " "_str;
        space_string.size = space_to_leave;
        BufferReplaceRange(buffer, MakeRange(start, end), space_string);
    }
}

COMMAND_PROC(AlignCursors, "Insert whitespaces before all the cursors so that they line up vertically"_str)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    bool all_cursors_have_same_alignment = true;
    int64_t align_col = -1;
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        // TODO: Handle tabs vs spaces?
        BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
        if (align_col != -1 && loc.col != align_col) all_cursors_have_same_alignment = false;
        if (align_col < loc.col)
        {
            align_col = loc.col;
        }
    }

    if (all_cursors_have_same_alignment)
    {
        CMD_UnalignCursors();
        return;
    }

    ScopedMemory temp;
    String padding_string = PushStringSpace(temp, align_col);
    for (size_t i = 0; i < padding_string.size; i += 1) padding_string.data[i] = ' ';

    BeginUndoBatch(buffer);
    for (Cursor *cursor = IterateCursors(view); cursor; cursor = cursor->next)
    {
        BufferLocation loc = CalculateBufferLocationFromPos(buffer, cursor->pos);
        int64_t padding = align_col - loc.col;
        Assert(padding >= 0);
        BufferReplaceRange(buffer, MakeRange(cursor->pos), MakeString((size_t)padding, padding_string.data));
    }
    EndUndoBatch(buffer);
}

TEXT_COMMAND_PROC(WriteText)
{
    View   *view   = GetActiveView();
    Buffer *buffer = GetBuffer(view);

    uint8_t buf_storage[256];
    StringContainer buf = MakeStringContainer(ArrayCount(buf_storage), buf_storage);

    size_t      size             = text.size;
    uint8_t    *data             = text.data;
    int         indent_width     = core_config->indent_width;
    bool        indent_with_tabs = core_config->indent_with_tabs;
    LineEndKind line_end_kind    = buffer->line_end;

    Cursor *cursor = GetCursor(view);
    int64_t insert_pos = cursor->pos;

    bool newline            = false;
    bool should_auto_indent = false;
    for (size_t i = 0; i < size; i += 1)
    {
        uint8_t c = data[i];

        if (c == '\n')
        {
            newline            = true;
            should_auto_indent = true;
        }

        if (!indent_with_tabs && c == '\t')
        {
            int left = (int)GetSizeLeft(&buf);

            int64_t line_start = FindLineStart(buffer, insert_pos);
            int64_t col = insert_pos - line_start;
            int64_t target_col = RoundUpNextTabStop(col, indent_width);

            int64_t spaces = target_col - col;
            Assert(spaces < left);

            AppendFill(&buf, spaces, ' ');
        }
        else if (newline && line_end_kind == LineEnd_CRLF)
        {
            Assert(CanFitAppend(&buf, "\r\n"_str));
            Append(&buf, "\r\n"_str);
        }
        else
        {
            Assert(GetSizeLeft(&buf) > 0);
            if (c == '\n' ||
                c == '\t' ||
                (c >= ' ' && c <= '~') ||
                c >= 128)
            {
                Append(&buf, c);
            }
        }
    }

    if (buf.size > 0)
    {
        if (newline && core_config->auto_line_comments)
        {
            LineInfo info;
            FindLineInfoByPos(buffer, insert_pos, &info);

            TokenIterator it = IterateLineTokens(&info);
            Token t = it.token;

            if (IsInRange(info.range, t.pos) && t.kind == Token_LineComment)
            {
                String line_comment_string = GetOperatorAsString(buffer->language, Token_LineComment);
                Append(&buf, line_comment_string);
                Append(&buf, ' ');
            }
        }

        BufferReplaceRange(buffer, MakeRange(insert_pos), buf.as_string);
        
        if (!should_auto_indent)
        {
            IndentRules *indent_rules = buffer->indent_rules;

            Token t = GetTokenAt(buffer, insert_pos);
            IndentRule rule = indent_rules->table[t.kind];
            if (rule & IndentRule_RequiresReindent)
            {
                should_auto_indent = true;
            }
        }

        if (should_auto_indent)
        {
            AutoIndentLineAt(buffer, cursor->pos);
        }
    }
}

COMMAND_PROC(SaveBuffer,
             "Save the current buffer to its respective file"_str)
{
    Buffer *buffer = GetActiveBuffer();
    platform->WriteFile(buffer->count, buffer->text, buffer->full_path);
    buffer->last_save_undo_ordinal = CurrentUndoOrdinal(buffer);
}

COMMAND_PROC(OpenNewLineBelow)
{
    View *view = GetActiveView();
    Buffer *buffer = GetBuffer(view);
    Cursor *cursor = GetCursor(view);

    CMD_EnterTextMode();

    int64_t insert_pos;
    FindLineEnd(buffer, cursor->pos, &insert_pos);

    SetCursor(cursor, insert_pos);
    CMD_WriteText("\n"_str); // NOTE: CRLF line endings are correctly expanded in WriteText
}

function void
OnMouseDown(void)
{
#if 0
    View *view = GetActiveView();
    Cursor *cursor = GetCursor(view);

    if (editor->token_at_mouse)
    {
        Token *t = editor->token_at_mouse;
        if (cursor->selection.inner.start == t->pos &&
            cursor->selection.inner.end   == t->pos + t->length)
        {
            if (cursor->pos == cursor->selection.inner.start)
            {
                cursor->pos = cursor->selection.inner.end;
            }
            else if (cursor->pos == cursor->selection.inner.end)
            {
                cursor->pos = cursor->selection.inner.start;
            }
        }
        else
        {
            cursor->pos = t->pos;
            cursor->selection = MakeSelection(MakeRangeStartLength(t->pos, t->length));
        }
    }
    else
    {
        cursor->pos = editor->pos_at_mouse;
        cursor->selection = MakeSelection(MakeRange(cursor->pos));
    }
#endif
}

function void
OnMouseHeld(void)
{
}

function void
OnMouseUp(void)
{
}
