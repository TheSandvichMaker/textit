#ifndef TEXTIT_COMMAND_HPP
#define TEXTIT_COMMAND_HPP

#define MAX_COMMAND_COUNT 1024

struct EditorState;
struct Cursor;
struct Cursors;

struct Selection
{
    Range inner;
    Range outer;
};

struct Selections
{
    size_t count;
    Selection *selections;

    Selection &operator[](size_t i)
    {
        Assert(i < count);
        return selections[i];
    }
};

function Selection
MakeSelection(int64_t pos)
{
    Selection result;
    result.inner.start = pos;
    result.outer.start = pos;
    result.inner.end   = pos + 1;
    result.outer.end   = pos + 1;
    return result;
}

function Selection
MakeSelection(Range inner, Range outer = {})
{
    if (outer.start == 0 &&
        outer.end   == 0)
    {
        outer = inner;
    }

    Selection result;
    result.inner = inner;
    result.outer = outer;

    return result;
}

function Selection
Union(const Selection &a, const Selection &b)
{
    Selection result;
    result.inner = Union(a.inner, b.inner);
    result.outer = Union(a.outer, b.outer);
    return result;
}

enum CommandKind
{
    Command_Basic,
    Command_Text,
    Command_Movement,
    Command_Change,
};

struct Move
{
    int64_t pos;
    Selection selection;
};

function Move
MakeMove(Range selection, int64_t pos = -1)
{
    Move result = {};
    result.selection.inner = selection;
    result.selection.outer = selection;
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

typedef void (*CommandProc)(void);
#define COMMAND_PROC(name, ...)                                                                                                             \
    static void Paste(CMD_, name)(void);                                                                                                    \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Basic, StringLiteral(#name), (void *)&Paste(CMD_, name), ##__VA_ARGS__);          \
    static void Paste(CMD_, name)(void)

typedef void (*TextCommandProc)(const Cursors &cursors, String text);
#define TEXT_COMMAND_PROC(name)                                                                                                             \
    static void Paste(CMD_, name)(const Cursors &cursors, String text);                                                                     \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Text, StringLiteral(#name), (void *)&Paste(CMD_, name));                          \
    static void Paste(CMD_, name)(const Cursors &cursors, String text)

typedef void (*MovementProc)(const Cursors &cursors);
#define MOVEMENT_PROC(name, ...)                                                                                                            \
    static void Paste(MOV_, name)(const Cursors &cursors);                                                                                  \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Movement, StringLiteral(#name), (void *)&Paste(MOV_, name), ""_str, __VA_ARGS__); \
    static void Paste(MOV_, name)(const Cursors &cursors)

typedef void (*ChangeProc)(const Cursors &cursors);
#define CHANGE_PROC(name)                                                                                                                   \
    static void Paste(CHG_, name)(const Cursors &cursors);                                                                                  \
    CommandRegisterHelper Paste(CMDHELPER_, name)(Command_Change, StringLiteral(#name), (void *)&Paste(CHG_, name));                        \
    static void Paste(CHG_, name)(const Cursors &cursors)

enum_flags(int, CommandFlags)
{
    Command_Visible       = 0x1,
    Command_Jump          = 0x2,
    Movement_NoAutoRepeat = 0x4,
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
CMD_Stub(void)
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
// Not using the global state macro because we want this to be reloaded if the DLL reloads
static CommandList command_list_, *command_list = &command_list_;

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
            if (command->kind == Command_Movement)
            {
                flags |= Command_Jump;
            }
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
    Modifier_None  = 0x0,
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
