function Command *
FindCommand(String name, StringMatchFlags flags)
{
    Command *result = NullCommand();
    for (size_t i = 0; i < command_list->command_count; i += 1)
    {
        Command *command = &command_list->commands[i];
        if (AreEqual(command->name, name, flags))
        {
            result = command;
            break;
        }
    }
    return result;
}

function void
BindCommand(BindingMap *map, PlatformInputCode code, Modifiers modifiers, String name)
{
    Command *command = FindCommand(name);

    Assert(modifiers < ArrayCount(map->by_code[0].by_modifiers));
    if (command->kind != Command_Text)
    {
        map->by_code[code].by_modifiers[modifiers] = command;
    }
    else
    {
        // TODO: complain
    }
}

function void
LoadDefaultBindings()
{
    BindingMap *command = &editor->bindings[EditMode_Command];
    command->text_command = nullptr;
    BindCommand(command, PlatformInputCode_Left,   Modifier_None,                "MoveLeft"_str);
    BindCommand(command, PlatformInputCode_Right,  Modifier_None,                "MoveRight"_str);
    BindCommand(command, PlatformInputCode_Left,   Modifier_Ctrl,                "MoveLeftIdentifier"_str);
    BindCommand(command, PlatformInputCode_Right,  Modifier_Ctrl,                "MoveRightIdentifier"_str);
    BindCommand(command, PlatformInputCode_Up,     Modifier_None,                "MoveUp"_str);
    BindCommand(command, PlatformInputCode_Down,   Modifier_None,                "MoveDown"_str);
    BindCommand(command, PlatformInputCode_Up,     Modifier_Ctrl,                "MoveUp"_str);
    BindCommand(command, PlatformInputCode_Down,   Modifier_Ctrl,                "MoveDown"_str);
    BindCommand(command, 'H',                      Modifier_None,                "MoveLeft"_str);
    BindCommand(command, 'J',                      Modifier_None,                "MoveDown"_str);
    BindCommand(command, 'J',                      Modifier_Ctrl,                "NewCursorDown"_str);
    BindCommand(command, 'K',                      Modifier_None,                "MoveUp"_str);
    BindCommand(command, 'K',                      Modifier_Ctrl,                "NewCursorUp"_str);
    BindCommand(command, 'L',                      Modifier_None,                "MoveRight"_str);
    BindCommand(command, 'W',                      Modifier_None,                "MoveRightIdentifier"_str);
    BindCommand(command, 'B',                      Modifier_None,                "MoveLeftIdentifier"_str);
    BindCommand(command, 'I',                      Modifier_None,                "EnterTextMode"_str);
    BindCommand(command, 'A',                      Modifier_None,                "Append"_str);
    BindCommand(command, 'A',                      Modifier_Shift,               "AppendAtEnd"_str);
    BindCommand(command, 'X',                      Modifier_None,                "DeleteChar"_str);
    BindCommand(command, 'U',                      Modifier_None,                "UndoOnce"_str);
    BindCommand(command, 'R',                      Modifier_Ctrl,                "RedoOnce"_str);

    BindCommand(command, PlatformInputCode_Space,  Modifier_None,                "QuickActionMenu"_str);

    BindCommand(command, 'A',                      Modifier_Ctrl,                "AlignCursors"_str);
    BindCommand(command, 'A',                      Modifier_Ctrl|Modifier_Shift, "UnalignCursors"_str);

    BindCommand(command, 'F',                      Modifier_None,                "FindChar"_str);
    BindCommand(command, 'F',                      Modifier_Shift,               "FindCharBackward"_str);
    BindCommand(command, 'T',                      Modifier_None,                "ToChar"_str);
    BindCommand(command, 'T',                      Modifier_Shift,               "ToCharBackward"_str);

    BindCommand(command, PlatformInputCode_Oem2,   Modifier_None,                "Search"_str);
    BindCommand(command, 'N',                      Modifier_None,                "RepeatLastSearch"_str);
    BindCommand(command, 'N',                      Modifier_Shift,               "RepeatLastSearchBackward"_str);

    BindCommand(command, 'O',                      Modifier_None,                "OpenNewLineBelow"_str);

    BindCommand(command, 'V',                      Modifier_Ctrl,                "SplitWindowVertical"_str);
    BindCommand(command, 'S',                      Modifier_Ctrl,                "SplitWindowHorizontal"_str);
    BindCommand(command, 'H',                      Modifier_Shift|Modifier_Ctrl, "FocusWindowLeft"_str);
    BindCommand(command, 'J',                      Modifier_Shift|Modifier_Ctrl, "FocusWindowDown"_str);
    BindCommand(command, 'K',                      Modifier_Shift|Modifier_Ctrl, "FocusWindowUp"_str);
    BindCommand(command, 'L',                      Modifier_Shift|Modifier_Ctrl, "FocusWindowRight"_str);
    BindCommand(command, 'Q',                      Modifier_Ctrl,                "DestroyWindow"_str);

    BindCommand(command, 'O',                      Modifier_Ctrl,                "PreviousJump"_str);
    BindCommand(command, 'I',                      Modifier_Ctrl,                "NextJump"_str);

    BindCommand(command, 'E',                      Modifier_Ctrl,                "OpenBuffer"_str);
    BindCommand(command, 'E',                      Modifier_Shift|Modifier_Ctrl, "OpenFile"_str);

    BindCommand(command, 'D',                      Modifier_None,                "DeleteSelection"_str);
    BindCommand(command, 'D',                      Modifier_Shift,               "DeleteInnerSelection"_str);
    BindCommand(command, 'C',                      Modifier_None,                "ChangeSelection"_str);
    BindCommand(command, 'C',                      Modifier_Shift,               "ChangeOuterSelection"_str);
    BindCommand(command, 'Q',                      Modifier_None,                "ToUppercase"_str);
    BindCommand(command, 'X',                      Modifier_None,                "EncloseLine"_str);
    BindCommand(command, 'F',                      Modifier_Ctrl,                "PageDown"_str);
    BindCommand(command, 'B',                      Modifier_Ctrl,                "PageUp"_str);
    BindCommand(command, 'Z',                      Modifier_None,                "CenterView"_str);
    BindCommand(command, 'G',                      Modifier_None,                "JumpToBufferStart"_str);
    BindCommand(command, 'G',                      Modifier_Shift,               "JumpToBufferEnd"_str);
    BindCommand(command, '0',                      Modifier_None,                "MoveLineStart"_str);
    BindCommand(command, '4',                      Modifier_Shift,               "MoveLineEnd"_str);
    BindCommand(command, 'S',                      Modifier_None,                "EncloseNextScope"_str);
    BindCommand(command, PlatformInputCode_Oem4,   Modifier_Shift,               "EncloseSurroundingScope"_str);
    BindCommand(command, PlatformInputCode_Oem6,   Modifier_Ctrl,                "GoToDefinitionUnderCursor"_str);
    BindCommand(command, '9',                      Modifier_Shift,               "EncloseSurroundingParen"_str);
    BindCommand(command, PlatformInputCode_Period, Modifier_None,                "RepeatLastCommand"_str);
    BindCommand(command, PlatformInputCode_F1,     Modifier_None,                "ToggleVisualizeNewlines"_str);
    BindCommand(command, PlatformInputCode_Comma,  Modifier_None,                "EncloseParameter"_str);

    BindCommand(command, PlatformInputCode_Escape, Modifier_None,                "ResetCursors"_str);

    BindCommand(command, 'Y',                      Modifier_None,                "Copy"_str);
    BindCommand(command, 'P',                      Modifier_Shift,               "PasteAfter"_str);
    BindCommand(command, 'P',                      Modifier_None,                "PasteBefore"_str);
    BindCommand(command, 'P',                      Modifier_Ctrl,                "PasteReplaceSelection"_str);

    BindCommand(command, PlatformInputCode_Oem1,   Modifier_Shift,               "EnterCommandLineMode"_str);

    BindingMap *text = &editor->bindings[EditMode_Text];
    text->text_command = FindCommand("WriteText"_str);
    BindCommand(text, PlatformInputCode_Left,   Modifier_None, "MoveLeft"_str);
    BindCommand(text, PlatformInputCode_Right,  Modifier_None, "MoveRight"_str);
    BindCommand(text, PlatformInputCode_Left,   Modifier_Ctrl, "MoveLeftIdentifier"_str);
    BindCommand(text, PlatformInputCode_Right,  Modifier_Ctrl, "MoveRightIdentifier"_str);
    BindCommand(text, PlatformInputCode_Up,     Modifier_None, "MoveUp"_str);
    BindCommand(text, PlatformInputCode_Down,   Modifier_None, "MoveDown"_str);
    BindCommand(text, PlatformInputCode_Up,     Modifier_Ctrl, "MoveUp"_str);
    BindCommand(text, PlatformInputCode_Down,   Modifier_Ctrl, "MoveDown"_str);
    BindCommand(text, PlatformInputCode_Escape, Modifier_None, "EnterCommandMode"_str);
    BindCommand(text, PlatformInputCode_Back,   Modifier_None, "BackspaceChar"_str);
    BindCommand(text, PlatformInputCode_Back,   Modifier_Ctrl, "BackspaceWord"_str);
    BindCommand(text, PlatformInputCode_Delete, Modifier_None, "DeleteChar"_str);
    BindCommand(text, PlatformInputCode_Delete, Modifier_Ctrl, "DeleteWord"_str);
}
