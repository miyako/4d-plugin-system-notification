#ifndef OWNERSHIP_HARNESS_H
#define OWNERSHIP_HARNESS_H

#include "4DPluginAPI.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"

#include <string>

namespace harness {

struct state_t {
    PA_long32 method_id = 0;
    bool method_returns_text = false;
    bool method_fails = false;
    int method_calls = 0;
    int command_calls = 0;
    long last_command = 0;
    PA_long32 last_event = -1;
    std::u16string last_method_name;
    int new_process_calls = 0;
    int run_in_main_calls = 0;
    int violations = 0;
    PA_Unichar poison = 0;
};

extern state_t state;

void reset();
size_t live_unistring_count();
size_t live_handle_count();
std::u16string to_u16(const PA_Unistring &u);

extern "C" void synthetic_call4d(short selector, EngineBlock *eb);

}

#endif
