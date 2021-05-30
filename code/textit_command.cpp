static inline Command *
FindCommand(String name)
{
    Command *result = NullCommand();
    for (size_t i = 0; i < command_list->command_count; i += 1)
    {
        Command *command = &command_list->commands[i];
        if (AreEqual(command->name, name))
        {
            result = command;
            break;
        }
    }
    return result;
}

static inline void
LoadDefaultBindings()
{
    BindingMap *command = &editor_state->bindings[EditMode_Command];
    command->text_command = nullptr;
    command->map[PlatformInputCode_Left].regular   = FindCommand("MoveLeft"_str);
    command->map[PlatformInputCode_Right].regular  = FindCommand("MoveRight"_str);
    command->map[PlatformInputCode_Left].ctrl      = FindCommand("MoveLeftIdentifier"_str);
    command->map[PlatformInputCode_Right].ctrl     = FindCommand("MoveRightIdentifier"_str);
    command->map[PlatformInputCode_Up].regular     = FindCommand("MoveUp"_str);
    command->map[PlatformInputCode_Down].regular   = FindCommand("MoveDown"_str);
    command->map[PlatformInputCode_Up].ctrl        = FindCommand("MoveUp"_str);
    command->map[PlatformInputCode_Down].ctrl      = FindCommand("MoveDown"_str);
    command->map['H'].regular                      = FindCommand("MoveLeft"_str);
    command->map['J'].regular                      = FindCommand("MoveDown"_str);
    command->map['K'].regular                      = FindCommand("MoveUp"_str);
    command->map['L'].regular                      = FindCommand("MoveRight"_str);
    command->map['W'].regular                      = FindCommand("MoveRightIdentifier"_str);
    command->map['B'].regular                      = FindCommand("MoveLeftIdentifier"_str);
    command->map['I'].regular                      = FindCommand("EnterTextMode"_str);
    command->map['X'].regular                      = FindCommand("DeleteChar"_str);
    command->map['U'].regular                      = FindCommand("UndoOnce"_str);
    command->map['R'].ctrl                         = FindCommand("RedoOnce"_str);
    command->map['V'].ctrl                         = FindCommand("SplitWindowVertical"_str);
    command->map['D'].regular                      = FindCommand("DeleteSelection"_str);
    command->map['C'].regular                      = FindCommand("ChangeSelection"_str);
    command->map['Q'].regular                      = FindCommand("ToUppercase"_str);
    command->map['X'].regular                      = FindCommand("EncloseLine"_str);
    command->map['F'].ctrl                         = FindCommand("PageDown"_str);
    command->map['B'].ctrl                         = FindCommand("PageUp"_str);
    command->map[PlatformInputCode_Period].regular = FindCommand("RepeatLastCommand"_str);

    BindingMap *text = &editor_state->bindings[EditMode_Text];
    text->text_command = FindCommand("WriteText"_str);
    text->map[PlatformInputCode_Left].regular     = FindCommand("MoveLeft"_str);
    text->map[PlatformInputCode_Right].regular    = FindCommand("MoveRight"_str);
    text->map[PlatformInputCode_Left].ctrl        = FindCommand("MoveLeftIdentifier"_str);
    text->map[PlatformInputCode_Right].ctrl       = FindCommand("MoveRightIdentifier"_str);
    text->map[PlatformInputCode_Up].regular       = FindCommand("MoveUp"_str);
    text->map[PlatformInputCode_Down].regular     = FindCommand("MoveDown"_str);
    text->map[PlatformInputCode_Up].ctrl          = FindCommand("MoveUp"_str);
    text->map[PlatformInputCode_Down].ctrl        = FindCommand("MoveDown"_str);
    text->map[PlatformInputCode_Escape].regular   = FindCommand("EnterCommandMode"_str);
    text->map[PlatformInputCode_Back].regular     = FindCommand("BackspaceChar"_str);
    text->map[PlatformInputCode_Back].ctrl        = FindCommand("BackspaceWord"_str);
    text->map[PlatformInputCode_Delete].regular   = FindCommand("DeleteChar"_str);
    text->map[PlatformInputCode_Delete].ctrl      = FindCommand("DeleteWord"_str);
}
