#ifndef TEXTIT_COMMAND_LINE_HPP
#define TEXTIT_COMMAND_LINE_HPP

struct Prediction
{
    String text;
    String preview_text;
    StringID color;
    uint8_t quickselect_char;
    bool incomplete;
    void *userdata;
};

function Prediction
MakePrediction(String text, StringID color = "command_line_option"_id, uint8_t quickselect_char = 0)
{
    Prediction result = {};
    result.text  = text;
    result.color = color;
    result.quickselect_char = quickselect_char;
    return result;
}

#define MAX_PREDICTIONS 8192

struct CommandLine
{
    Arena *arena;
    TemporaryMemory temporary_memory;

    void *userdata;

    String name;

    bool terminate;
    bool accepted_entry;

    bool no_quickselect;
    bool no_autoaccept;
    bool sort_by_edit_distance;
    bool auto_accept;

    int cursor;
    int count;
    uint8_t text[256];

    bool highlight_numbers;

    bool cycling_predictions;
    int prediction_index;
    int prediction_selected_index;
    int prediction_count;
    bool actively_scrolling;
    int scroll_offset;
    bool prediction_overflow;
    Prediction predictions[MAX_PREDICTIONS];
    SortKey sort_keys[MAX_PREDICTIONS];

    void (*GatherPredictions)(CommandLine *cl);
    String (*OnText)(CommandLine *cl, String text);
    void (*OnTerminate)(CommandLine *cl);
    bool (*AcceptEntry)(CommandLine *cl);
};

function CommandLine *BeginCommandLine();
function void EndCommandLine();
function void EndAllCommandLines();
function bool HandleCommandLineEvent(CommandLine *cl, PlatformEvent *event);
function String GetCommandString(CommandLine *cl);
function bool AddPrediction(CommandLine *cl, const Prediction &prediction, uint32_t sort_key = 0);
function Prediction *GetPrediction(CommandLine *cl, int index = -1);

function void
Terminate(CommandLine *cl)
{
    cl->terminate = true;
}

#endif /* TEXTIT_COMMAND_LINE_HPP */
