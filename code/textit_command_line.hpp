#ifndef TEXTIT_COMMAND_LINE_HPP
#define TEXTIT_COMMAND_LINE_HPP

struct CommandLine
{
    Arena *arena;
    String name;

    bool no_quickselect;

    int cursor;
    int count;
    uint8_t text[256];

    bool cycling_predictions;
    int prediction_index;
    int prediction_selected_index;
    int prediction_count;
    String predictions[32];

    void (*GatherPredictions)(CommandLine *cl);
    bool (*AcceptEntry)(CommandLine *cl);
};

function CommandLine *BeginCommandLine();
function void EndCommandLine();
function bool AddPrediction(CommandLine *cl, const String &prediction);

#endif /* TEXTIT_COMMAND_LINE_HPP */
