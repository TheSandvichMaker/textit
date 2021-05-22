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
