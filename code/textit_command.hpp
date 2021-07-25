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

enum_flags(int, MoveFlags)
{
    MoveFlag_NoAutoRepeat = 0x1,
};

struct Move
{
    Range inner_selection;
    Range outer_selection;
    int64_t pos;
    MoveFlags flags;
};

function Move
MakeMove(Range selection, int64_t pos = -1)
{
    Move result = {};
    result.inner_selection = selection;
    result.outer_selection = selection;
    result.pos             = pos;
    if (pos < 0)
    {
        result.pos = selection.end;
    }
    return result;
}

function Move
MakeMove(int64_t pos)
{
    return MakeMove(MakeRange(pos), pos);
}

typedef void (*CommandProc)(EditorState *editor);
#define COMMAND_PROC(name, ...)                                                                                                    \
    static void Paste(CMD_, name)(EditorState *editor);                                                                            \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Basic, StringLiteral(#name), (void *)&Paste(CMD_, name), ##__VA_ARGS__); \
    static void Paste(CMD_, name)(EditorState *editor)

typedef void (*TextCommandProc)(EditorState *editor, String text);
#define TEXT_COMMAND_PROC(name)                                                                                                    \
    static void Paste(CMD_, name)(EditorState *editor, String text);                                                               \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Text, StringLiteral(#name), (void *)&Paste(CMD_, name));                 \
    static void Paste(CMD_, name)(EditorState *editor, String text)

typedef Move (*MovementProc)(EditorState *editor);
#define MOVEMENT_PROC(name)                                                                                                        \
    static Move Paste(MOV_, name)(EditorState *editor);                                                                            \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Movement, StringLiteral(#name), (void *)&Paste(MOV_, name));             \
    static Move Paste(MOV_, name)(EditorState *editor)

typedef void (*ChangeProc)(EditorState *editor, Range inner_range, Range outer_range);
#define CHANGE_PROC(name)                                                                                                          \
    static void Paste(CHG_, name)(EditorState *editor, Range inner_range, Range outer_range);                                      \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Change, StringLiteral(#name), (void *)&Paste(CHG_, name));               \
    static void Paste(CHG_, name)(EditorState *editor, Range inner_range, Range outer_range)

enum_flags(int, CommandFlags)
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

function void
CMD_Stub(EditorState *)
{
    platform->DebugPrint("Someone called the stub command.\n");
}

function Command
MakeNullCommand()
{
    Command result = {};
    result.name = "Stub"_str;
    result.description = "It's the stub command, if this gets called somebody did something wrong"_str;
    result.command = CMD_Stub;
    return result;
}

static Command null_command_ = MakeNullCommand();

function Command *
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

function Command *FindCommand(String name, StringMatchFlags flags = 0);

struct CommandRegisterHelper
{
    CommandRegisterHelper(CommandKind kind, String name, void *proc, String description = ""_str, CommandFlags flags = 0)
    {
        Assert(FindCommand(name) == NullCommand());
        if (description.size > 0)
        {
            flags |= Command_Visible;
        }
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

enum_flags(int, Modifiers)
{
    Modifier_Alt   = 0x1,
    Modifier_Shift = 0x2,
    Modifier_Ctrl  = 0x4,
};

struct Binding
{
    Command *by_modifiers[9];
};

struct BindingMap
{
    Command *text_command;
    Binding by_code[PlatformInputCode_COUNT];
};

#endif /* TEXTIT_COMMAND_HPP */
