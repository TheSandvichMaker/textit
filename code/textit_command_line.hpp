#ifndef TEXTIT_COMMAND_LINE_HPP
#define TEXTIT_COMMAND_LINE_HPP

struct Prediction
{
    String text;
    String preview_text;
    StringID color;
    bool incomplete;
};

function Prediction
MakePrediction(String text, StringID color = "command_line_option"_id)
{
    Prediction result = {};
    result.text     = text;
    result.color    = color;
    return result;
}

struct CommandLine
{
    Arena *arena;
    TemporaryMemory temporary_memory;

    String name;

    bool no_quickselect;
    bool no_autoaccept;

    int cursor;
    int count;
    uint8_t text[256];

    bool highlight_numbers;

    bool cycling_predictions;
    int prediction_index;
    int prediction_selected_index;
    int prediction_count;
    Prediction predictions[35];
    SortKey sort_keys[35];

    void (*GatherPredictions)(CommandLine *cl);
    bool (*AcceptEntry)(CommandLine *cl);
};

function CommandLine *BeginCommandLine();
function void EndCommandLine();
function bool AddPrediction(CommandLine *cl, const Prediction &prediction);

#endif /* TEXTIT_COMMAND_LINE_HPP */
