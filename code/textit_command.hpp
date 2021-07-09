#ifndef TEXTIT_COMMAND_HPP
#define TEXTIT_COMMAND_HPP

#define MAX_COMMAND_COUNT 1024

struct EditorState;

enum CommandKind
{
    Command_Basic,
    Command_Text,
    Command_Movement,
    Command_Change,
};

typedef void (*CommandProc)(EditorState *editor);
#define COMMAND_PROC(name, ...)                                                                                           \
    static void Paste(CMD_, name)(EditorState *editor);                                                                   \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Basic, StringLiteral(#name), Paste(CMD_, name), ##__VA_ARGS__); \
    static void Paste(CMD_, name)(EditorState *editor)

typedef void (*TextCommandProc)(EditorState *editor, String text);
#define TEXT_COMMAND_PROC(name)                                                                                           \
    static void Paste(CMD_, name)(EditorState *editor, String text);                                                      \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Text, StringLiteral(#name), Paste(CMD_, name));                 \
    static void Paste(CMD_, name)(EditorState *editor, String text)

typedef Range (*MovementProc)(EditorState *editor);
#define MOVEMENT_PROC(name)                                                                                               \
    static Range Paste(MOV_, name)(EditorState *editor);                                                                  \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Movement, StringLiteral(#name), Paste(MOV_, name));             \
    static Range Paste(MOV_, name)(EditorState *editor)

typedef void (*ChangeProc)(EditorState *editor, Range range);
#define CHANGE_PROC(name)                                                                                                 \
    static void Paste(CHG_, name)(EditorState *editor, Range range);                                                      \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Change, StringLiteral(#name), Paste(CHG_, name));               \
    static void Paste(CHG_, name)(EditorState *editor, Range range)

typedef uint32_t CommandFlags;
enum CommandFlags_ENUM : CommandFlags
{
    Command_Visible = 0x1,
};

struct Command
{
    CommandKind kind;
    CommandFlags flags;
    String name;
    String description;
    union
    {
        void *generic;
        CommandProc command;
        TextCommandProc text;
        MovementProc movement;
        ChangeProc change;
    };
};

static inline void
CMD_Stub(EditorState *)
{
    platform->DebugPrint("Someone called the stub command.\n");
}
static Command null_command_ = { Command_Basic, 0, "Stub"_str, "It's the stub command, if this gets called somebody did something wrong"_str, CMD_Stub };
static inline Command *
NullCommand()
{
    return &null_command_;
}

struct CommandList
{
    uint32_t command_count;
    Command commands[MAX_COMMAND_COUNT];
};
GLOBAL_STATE(CommandList, command_list);

static inline Command *FindCommand(String name);

struct CommandRegisterHelper
{
    CommandRegisterHelper(CommandKind kind, String name, void *proc, String description = ""_str, CommandFlags flags = 0)
    {
        Assert(FindCommand(name) == NullCommand());
        if (command_list->command_count < ArrayCount(command_list->commands))
        {
            Command *command = &command_list->commands[command_list->command_count++];
            command->kind        = kind;
            command->flags       = flags;
            command->name        = name;
            command->description = description;
            command->generic     = proc;
        }
        else
        {
            INVALID_CODE_PATH;
        }
    }
};
#define REGISTER_COMMAND(name) CommandRegisterHelper Paste(command_, __COUNTER__)(StringLiteral(#name), name);

enum BindingModifier
{
    BindMod_None,
    BindMod_Ctrl,
};

struct Binding
{
    Command *regular;
    Command *ctrl;
    Command *shift;
    Command *ctrl_shift;
};

struct BindingKey
{
    PlatformInputCode code;
    BindingModifier mod;
};

static inline BindingKey
MakeBindingKey(PlatformInputCode code, BindingModifier mod = BindMod_None)
{
    BindingKey result;
    result.code = code;
    result.mod = mod;
    return result;
}

struct BindingMap
{
    Command *text_command;
    Binding map[PlatformInputCode_COUNT];
};

#endif /* TEXTIT_COMMAND_HPP */
