#ifndef TEXTIT_GLOBAL_STATE_HPP
#define TEXTIT_GLOBAL_STATE_HPP

#define MAX_GLOBAL_STATE 256

struct GlobalStateEntry
{
    String guid;
    void **variable_at;
    size_t data_size;
    size_t data_align;
    void *prototype;
    void *data;
};

struct GlobalState
{
    size_t entry_count;
    GlobalStateEntry entries[MAX_GLOBAL_STATE];
};

static GlobalState new_global_state;
static GlobalState *global_state = &new_global_state;

#define CreateGlobalState(type, variable_at, prototype) \
    (type *)CreateGlobalState_(sizeof(type), alignof(type), variable_at, prototype, StringLiteral(#type " " #variable_at))
static inline void *CreateGlobalState_(size_t size, size_t align, void *variable_at, void *prototype, String guid);

#if TEXTIT_BUILD_DLL
#define GLOBAL_STATE(type, variable_name) \
    static type Paste(variable_name, _prototype_); \
    static type *variable_name = (CreateGlobalState(type, &variable_name, &Paste(variable_name, _prototype_)), &Paste(variable_name, _prototype_));
#else
#define GLOBAL_STATE(type, variable_name) \
    static type Paste(variable_name, _);  \
    static type *variable_name = &Paste(variable_name, _);
#endif

#endif /* TEXTIT_GLOBAL_STATE_HPP */
