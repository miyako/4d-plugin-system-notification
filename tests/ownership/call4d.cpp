/*
 Synthetic Call4D for the ownership harness.

 Models the 4D engine just enough to drive the plugin's real sources through the
 unchanged SDK 4DPluginAPI.c, and keeps a ledger of every engine-owned
 PA_Unistring buffer so that leaks, double frees and frees of foreign memory are
 detected deterministically (ASan/UBSan additionally catch memory errors).

 No OS notification API is touched: PA_RunInMainProcess (which would install the
 NSWorkspace/NSDistributedNotificationCenter observers) and PA_NewProcess are
 recorded but never executed.
 */

#include "harness.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace harness {

state_t state;

static std::set<PA_Unichar *> live_unistrings;
static std::set<char **> live_handles;

struct handle_t {
    char *data;
    long size;
};

static void violation(const char *what) {
    state.violations++;
    fprintf(stderr, "    ledger violation: %s\n", what);
}

static size_t ulen(const PA_Unichar *s) {
    size_t n = 0;
    if (s) while (s[n]) n++;
    return n;
}

static PA_Unistring make_unistring(const PA_Unichar *src) {
    size_t n = ulen(src);
    PA_Unistring u;
    memset(&u, 0, sizeof(u));
    /* SDK structs are 2-byte packed: never bind references to their pointer fields */
    PA_Unichar *buf = (PA_Unichar *)calloc(n + 1, sizeof(PA_Unichar));
    if (n) memcpy(buf, src, n * sizeof(PA_Unichar));
    u.fString = buf;
    u.fLength = (PA_long32)n;
    live_unistrings.insert(buf);
    return u;
}

static void free_unistring(PA_Unistring *u, const char *ctx) {
    PA_Unichar *buf = u->fString;
    if (!buf) return;
    if (!live_unistrings.erase(buf)) {
        violation(ctx);
        return;
    }
    free(buf);
    u->fString = NULL;
    u->fLength = 0;
}

std::u16string to_u16(const PA_Unistring &u) {
    return std::u16string((const char16_t *)u.fString, (size_t)u.fLength);
}

size_t live_unistring_count() { return live_unistrings.size(); }
size_t live_handle_count() { return live_handles.size(); }

void reset() {
    state = state_t();
}

static void handle_manager(EngineBlock *eb) {
    switch (eb->fParam1) {
        case 1: {
            handle_t *h = (handle_t *)calloc(1, sizeof(handle_t));
            h->size = (long)eb->fParam2;
            h->data = (char *)calloc(1, h->size ? h->size : 1);
            PA_Handle ph = (PA_Handle)&h->data;
            eb->fHandle = ph;
            live_handles.insert(ph);
            break;
        }
        case 2: {
            PA_Handle ph = eb->fHandle;
            if (!live_handles.erase(ph)) { violation("dispose of unknown handle"); break; }
            handle_t *h = (handle_t *)eb->fHandle;
            free(h->data);
            free(h);
            break;
        }
        case 4:
            eb->fParam2 = ((handle_t *)eb->fHandle)->size;
            break;
        case 5:
            eb->fParam3 = (sLONG_PTR)((handle_t *)eb->fHandle)->data;
            break;
        default:
            break;
    }
}

static void clear_variable(PA_Variable *v) {
    switch (v->fType) {
        case eVK_Unistring:
            free_unistring(&v->uValue.fString, "PA_ClearVariable of a string the engine does not own");
            break;
        default:
            break;
    }
    v->fType = eVK_Undefined;
}

static void call_by_procid(EngineBlock *eb) {
    state.method_calls++;
    PA_Variable *pt = (PA_Variable *)((handle_t *)eb->fHandle)->data;
    PA_Variable *params = pt + 1;
    if (eb->fParam2 >= 1 && params[0].fType == eVK_Longint)
        state.last_event = params[0].uValue.fLongint;
    if (state.method_fails) {
        /* on error the returned slot is not a valid engine value: clearing it is a defect */
        eb->fError = -1;
        pt[0].fType = eVK_Unistring;
        pt[0].uValue.fString.fString = (PA_Unichar *)&state.poison;
        pt[0].uValue.fString.fLength = 1;
        return;
    }
    if (state.method_returns_text) {
        const PA_Unichar r[] = {'o', 'k', 0};
        pt[0].fType = eVK_Unistring;
        pt[0].uValue.fString = make_unistring(r);
    }
}

static void execute_command(EngineBlock *eb) {
    PA_Variable **params = (PA_Variable **)eb->fPtr1;
    PA_Variable *ret = (PA_Variable *)eb->fPtr2;
    state.command_calls++;
    state.last_command = (long)eb->fParam1;
    if (eb->fParam1 == 1007 /* EXECUTE METHOD without "*": returns Undefined */) {
        if (eb->fParam2 >= 1 && params[0]->fType == eVK_Unistring)
            state.last_method_name = to_u16(params[0]->uValue.fString);
        if (eb->fParam2 >= 2 && params[1]->fType == eVK_Longint)
            state.last_event = params[1]->uValue.fLongint;
        ret->fType = eVK_Undefined;
    }
}

extern "C" void synthetic_call4d(short selector, EngineBlock *eb) {
    switch (selector) {
        case EX_HANDLE_MANAGER: handle_manager(eb); break;
        case EX_CREATE_UNISTRING: eb->fUniString1 = make_unistring((PA_Unichar *)eb->fHandle); break;
        case EX_DISPOSE_UNISTRING: free_unistring(&eb->fUniString1, "PA_DisposeUnistring of a string the engine does not own"); break;
        case EX_SET_UNISTRING: {
            free_unistring(&eb->fUniString1, "PA_SetUnistring on a string the engine does not own");
            eb->fUniString1 = make_unistring((PA_Unichar *)eb->fHandle);
            break;
        }
        case EX_CLEAR_VARIABLE: clear_variable((PA_Variable *)eb->fPtr1); break;
        case EX_GET_METHOD_ID: eb->fParam1 = state.method_id; break;
        case EX_CALL_BY_PROCID: call_by_procid(eb); break;
        case EX_EXECUTE_COMMAND_BY_ID: execute_command(eb); break;
        case EX_CURRENT_PROCESS_NUMBER: eb->fParam1 = 2; break;
        case EX_GET_PROCESS_INFO: {
            const char *n = "Harness";
            size_t i = 0;
            for (; n[i]; i++) eb->fUString[i] = (PA_Unichar)n[i];
            eb->fUString[i] = 0;
            break;
        }
        case EX_NEW_PROCESS: state.new_process_calls++; eb->fParam1 = 7; break;
        case EX_RUN_IN_MAIN_PROCESS: state.run_in_main_calls++; break;
        default: break;
    }
}

}
