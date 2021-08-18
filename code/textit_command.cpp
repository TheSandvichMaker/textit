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
    BindCommand(command, PlatformInputCode_Left,   0,              "MoveLeft"_str);
    BindCommand(command, PlatformInputCode_Right,  0,              "MoveRight"_str);
    BindCommand(command, PlatformInputCode_Left,   Modifier_Ctrl,  "MoveLeftIdentifier"_str);
    BindCommand(command, PlatformInputCode_Right,  Modifier_Ctrl,  "MoveRightIdentifier"_str);
    BindCommand(command, PlatformInputCode_Up,     0,              "MoveUp"_str);
    BindCommand(command, PlatformInputCode_Down,   0,              "MoveDown"_str);
    BindCommand(command, PlatformInputCode_Up,     Modifier_Ctrl,  "MoveUp"_str);
    BindCommand(command, PlatformInputCode_Down,   Modifier_Ctrl,  "MoveDown"_str);
    BindCommand(command, 'H',                      0,              "MoveLeft"_str);
    BindCommand(command, 'J',                      0,              "MoveDown"_str);
    BindCommand(command, 'K',                      0,              "MoveUp"_str);
    BindCommand(command, 'L',                      0,              "MoveRight"_str);
    BindCommand(command, 'W',                      0,              "MoveRightIdentifier"_str);
    BindCommand(command, 'B',                      0,              "MoveLeftIdentifier"_str);
    BindCommand(command, 'I',                      0,              "EnterTextMode"_str);
    BindCommand(command, 'A',                      0,              "Append"_str);
    BindCommand(command, 'A',                      Modifier_Shift, "AppendAtEnd"_str);
    BindCommand(command, 'X',                      0,              "DeleteChar"_str);
    BindCommand(command, 'U',                      0,              "UndoOnce"_str);
    BindCommand(command, 'R',                      Modifier_Ctrl,  "RedoOnce"_str);

    BindCommand(command, 'O',                      0,              "OpenNewLineBelow"_str);

    BindCommand(command, 'V',                      Modifier_Ctrl,  "SplitWindowVertical"_str);
    BindCommand(command, 'S',                      Modifier_Ctrl,  "SplitWindowHorizontal"_str);
    BindCommand(command, 'H',                      Modifier_Ctrl,  "FocusWindowLeft"_str);
    BindCommand(command, 'J',                      Modifier_Ctrl,  "FocusWindowDown"_str);
    BindCommand(command, 'K',                      Modifier_Ctrl,  "FocusWindowUp"_str);
    BindCommand(command, 'L',                      Modifier_Ctrl,  "FocusWindowRight"_str);
    BindCommand(command, 'Q',                      Modifier_Ctrl,  "DestroyWindow"_str);

    BindCommand(command, 'O',                      Modifier_Ctrl,                "OpenBuffer"_str);
    BindCommand(command, 'O',                      Modifier_Shift|Modifier_Ctrl, "OpenFile"_str);

    BindCommand(command, 'D',                      0,              "DeleteSelection"_str);
    BindCommand(command, 'D',                      Modifier_Shift, "DeleteInnerSelection"_str);
    BindCommand(command, 'C',                      0,              "ChangeSelection"_str);
    BindCommand(command, 'C',                      Modifier_Shift, "ChangeOuterSelection"_str);
    BindCommand(command, 'Q',                      0,              "ToUppercase"_str);
    BindCommand(command, 'X',                      0,              "EncloseLine"_str);
    BindCommand(command, 'F',                      Modifier_Ctrl,  "PageDown"_str);
    BindCommand(command, 'B',                      Modifier_Ctrl,  "PageUp"_str);
    BindCommand(command, 'Z',                      0,              "CenterView"_str);
    BindCommand(command, 'G',                      0,              "JumpToBufferStart"_str);
    BindCommand(command, 'G',                      Modifier_Shift, "JumpToBufferEnd"_str);
    BindCommand(command, '0',                      0,              "MoveLineStart"_str);
    BindCommand(command, '4',                      Modifier_Shift, "MoveLineEnd"_str);
    BindCommand(command, 'S',                      0,              "EncloseNextScope"_str);
    BindCommand(command, PlatformInputCode_Oem4,   Modifier_Shift, "EncloseSurroundingScope"_str);
    BindCommand(command, PlatformInputCode_Oem6,   Modifier_Ctrl,  "GoToDefinitionUnderCursor"_str);
    BindCommand(command, '9',                      Modifier_Shift, "EncloseSurroundingParen"_str);
    BindCommand(command, PlatformInputCode_Period, 0,              "RepeatLastCommand"_str);
    BindCommand(command, PlatformInputCode_F1,     0,              "ToggleVisualizeNewlines"_str);
    BindCommand(command, PlatformInputCode_Comma,  0,              "EncloseParameter"_str);

    BindCommand(command, PlatformInputCode_Escape, 0,              "ResetSelection"_str);

    BindCommand(command, 'Y',                      0,              "Copy"_str);
    BindCommand(command, 'P',                      Modifier_Shift, "PasteAfter"_str);
    BindCommand(command, 'P',                      0,              "PasteBefore"_str);
    BindCommand(command, 'P',                      Modifier_Ctrl,  "PasteReplaceSelection"_str);

    BindCommand(command, PlatformInputCode_Oem1,   Modifier_Shift, "EnterCommandLineMode"_str);

    BindingMap *text = &editor->bindings[EditMode_Text];
    text->text_command = FindCommand("WriteText"_str);
    BindCommand(text, PlatformInputCode_Left,   0,             "MoveLeft"_str);
    BindCommand(text, PlatformInputCode_Right,  0,             "MoveRight"_str);
    BindCommand(text, PlatformInputCode_Left,   Modifier_Ctrl, "MoveLeftIdentifier"_str);
    BindCommand(text, PlatformInputCode_Right,  Modifier_Ctrl, "MoveRightIdentifier"_str);
    BindCommand(text, PlatformInputCode_Up,     0,             "MoveUp"_str);
    BindCommand(text, PlatformInputCode_Down,   0,             "MoveDown"_str);
    BindCommand(text, PlatformInputCode_Up,     Modifier_Ctrl, "MoveUp"_str);
    BindCommand(text, PlatformInputCode_Down,   Modifier_Ctrl, "MoveDown"_str);
    BindCommand(text, PlatformInputCode_Escape, 0,             "EnterCommandMode"_str);
    BindCommand(text, PlatformInputCode_Back,   0,             "BackspaceChar"_str);
    BindCommand(text, PlatformInputCode_Back,   Modifier_Ctrl, "BackspaceWord"_str);
    BindCommand(text, PlatformInputCode_Delete, 0,             "DeleteChar"_str);
    BindCommand(text, PlatformInputCode_Delete, Modifier_Ctrl, "DeleteWord"_str);
}
