#ifndef TEXTIT_COMMAND_HPP
#define TEXTIT_COMMAND_HPP

#define MAX_COMMAND_COUNT 1024

struct EditorState;

typedef void (*CommandProc)(EditorState *editor);
typedef void (*TextCommandProc)(EditorState *editor, String text);
struct Command
{
    String name;
    union
    {
        void *generic;
        CommandProc proc;
        TextCommandProc text_proc;
    };
};

static inline void
CMD_Stub(EditorState *)
{
    platform->DebugPrint("Someone called the stub command.\n");
}
static Command null_command_ = { StringLiteral("CMD_Stub"), CMD_Stub };
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
    CommandRegisterHelper(String name, void *proc)
    {
        Assert(FindCommand(name) == NullCommand());
        if (command_list->command_count < ArrayCount(command_list->commands))
        {
            Command *command = &command_list->commands[command_list->command_count++];
            command->name = name;
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
