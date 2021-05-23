#ifndef TEXTIT_COMMAND_HPP
#define TEXTIT_COMMAND_HPP

#define MAX_COMMAND_COUNT 1024

struct EditorState;

enum CommandKind
{
    Command_Basic,
    Command_Text,
    Command_Movement,
};

typedef void (*CommandProc)(EditorState *editor);
#define COMMAND_PROC(name, ...)                                                                                    \
    static void Paste(CMD_, name)(EditorState *editor);                                                       \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Basic, StringLiteral(#name), Paste(CMD_, name), ##__VA_ARGS__);    \
    static void Paste(CMD_, name)(EditorState *editor)

typedef void (*TextCommandProc)(EditorState *editor, String text);
#define TEXT_COMMAND_PROC(name)                                                                               \
    static void Paste(CMD_, name)(EditorState *editor, String text);                                          \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Text, StringLiteral(#name), Paste(CMD_, name));     \
    static void Paste(CMD_, name)(EditorState *editor, String text)

typedef Range (*MovementProc)(EditorState *editor);
#define MOVEMENT_PROC(name)                                                                               \
    static Range Paste(MOV_, name)(EditorState *editor);                                          \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Movement, StringLiteral(#name), Paste(MOV_, name)); \
    static Range Paste(MOV_, name)(EditorState *editor)

struct Command
{
    CommandKind kind;
    String name;
    String description;
    union
    {
        void *generic;
        CommandProc command;
        TextCommandProc text;
        MovementProc movement;
    };
};

static inline void
CMD_Stub(EditorState *)
{
    platform->DebugPrint("Someone called the stub command.\n");
}
static Command null_command_ = { Command_Basic, "Stub"_str, "It's the stub command, if this gets called somebody did something wrong"_str, CMD_Stub };
static inline Command *
NullCommand()
{
    return &null_command_;
}

struct CommandList
{
    uint32_t command_count;
    Command commands[MAX_COMMAND_COUNT*4 / 3];
};
GLOBAL_STATE(CommandList, command_list);

static inline Command *FindCommand(String name);

struct CommandRegisterHelper
{
    CommandRegisterHelper(CommandKind kind, String name, void *proc, String description = ""_str)
    {
        Assert(FindCommand(name) == NullCommand());
        if (command_list->command_count < ArrayCount(command_list->commands))
        {
            Command *command = &command_list->commands[command_list->command_count++];
            command->name = name;
            command->description = description;
            command->kind = kind;
            command->generic = proc;
        }
        else
        {
            INVALID_CODE_PATH;
        }
    }
};
#define REGISTER_COMMAND(name) CommandRegisterHelper Paste(command_, __COUNTER__)(StringLiteral(#name), name);

struct Binding
{
    Command *regular;
    Command *ctrl;
    Command *shift;
    Command *ctrl_shift;
};

struct BindingMap
{
    Command *text_command;
    Binding map[PlatformInputCode_COUNT];
};

#endif /* TEXTIT_COMMAND_HPP */
