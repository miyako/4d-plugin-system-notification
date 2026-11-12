/*
 Ownership regression tests for System Notification.

 Links the real 4DPlugin-System-Notification.cpp, C_TEXT.cpp, C_LONGINT.cpp and
 the unchanged SDK 4DPluginAPI.c against a synthetic Call4D (call4d.cpp).
 */

#include "harness.h"
#include "C_TEXT.h"

#include <cstdio>
#include <cstring>
#include <vector>

typedef PA_long32 event_id_t;
namespace SN {
    extern C_TEXT LISTENER_METHOD;
    extern std::vector<event_id_t> CALLBACK_EVENT_IDS;
}
void listenerLoopExecuteMethod(void);
void PluginMain(PA_long32 selector, PA_PluginParameters params);

using namespace harness;

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "    CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ok = false; } } while (0)

static PA_Unistring engine_string(const char16_t *s) {
    return PA_CreateUnistring((PA_Unichar *)s);
}

static void sn_set_method(const char16_t *name) {
    PA_Unistring arg = engine_string(name);
    void *params[1] = { &arg };
    sLONG_PTR result = 0;
    PluginBlock pb;
    pb.fParameters = params;
    pb.fResult = &result;
    pb.fData = NULL;
    PluginMain(1, &pb);
    PA_DisposeUnistring(&arg);
}

static std::u16string sn_get_method() {
    PA_Unistring ret = engine_string(u"");
    PA_Unistring *pret = &ret;
    PluginBlock pb;
    pb.fParameters = NULL;
    pb.fResult = &pret;
    pb.fData = NULL;
    PluginMain(2, &pb);
    std::u16string s = to_u16(ret);
    PA_DisposeUnistring(&ret);
    return s;
}

static void fire(event_id_t event) {
    SN::CALLBACK_EVENT_IDS.push_back(event + 1);
    listenerLoopExecuteMethod();
}

static void finish(const char *name, bool ok, size_t live_before) {
    if (live_unistring_count() != live_before) {
        fprintf(stderr, "    leaked engine strings: %zu\n", live_unistring_count() - live_before);
        ok = false;
    }
    if (live_handle_count() != 0) {
        fprintf(stderr, "    leaked handles: %zu\n", live_handle_count());
        ok = false;
    }
    if (state.violations) ok = false;
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

/* DEFECT: the EXECUTE METHOD (1007) fallback created an eVK_Unistring variable and
   then shallow-set the method name over it, orphaning the empty engine string. */
static void defect_execute_method_fallback_no_leak() {
    bool ok = true;
    reset();
    sn_set_method(u"Component_callback");
    size_t before = live_unistring_count();
    state.method_id = 0;
    for (int i = 0; i < 3; i++) fire(4);
    CHECK(state.command_calls == 3);
    CHECK(state.last_command == 1007);
    CHECK(state.last_method_name == u"Component_callback");
    CHECK(state.last_event == 4);
    finish("defect_execute_method_fallback_no_leak", ok, before);
}

/* DEFECT: the value returned by PA_ExecuteMethodByID is owned by the caller and
   was discarded without being cleared. */
static void defect_method_result_cleared() {
    bool ok = true;
    reset();
    sn_set_method(u"Project_callback");
    size_t before = live_unistring_count();
    state.method_id = 42;
    state.method_returns_text = true;
    for (int i = 0; i < 3; i++) fire(2);
    CHECK(state.method_calls == 3);
    CHECK(state.last_event == 2);
    finish("defect_method_result_cleared", ok, before);
}

/* CONTROL: on error the returned slot must not be cleared (gate on PA_GetLastError). */
static void control_method_error_not_cleared() {
    bool ok = true;
    reset();
    sn_set_method(u"Project_callback");
    size_t before = live_unistring_count();
    state.method_id = 42;
    state.method_fails = true;
    fire(1);
    CHECK(state.method_calls == 1);
    finish("control_method_error_not_cleared", ok, before);
}

/* CONTROL: callback method without a return value. */
static void control_method_no_result() {
    bool ok = true;
    reset();
    sn_set_method(u"Project_callback");
    size_t before = live_unistring_count();
    state.method_id = 42;
    fire(7);
    CHECK(state.method_calls == 1);
    CHECK(state.last_event == 7);
    finish("control_method_no_result", ok, before);
}

/* CONTROL: SN Set method / SN Get method round-trip through PluginMain, without
   starting any OS listener (PA_RunInMainProcess / PA_NewProcess are recorded only). */
static void control_set_get_method_roundtrip() {
    bool ok = true;
    reset();
    size_t before = live_unistring_count();
    sn_set_method(u"My_method");
    CHECK(sn_get_method() == u"My_method");
    sn_set_method(u"Other");
    CHECK(sn_get_method() == u"Other");
    CHECK(state.run_in_main_calls == 2);
    finish("control_set_get_method_roundtrip", ok, before);
}

/* CONTROL: an empty queue never calls back into 4D. */
static void control_empty_queue() {
    bool ok = true;
    reset();
    size_t before = live_unistring_count();
    SN::CALLBACK_EVENT_IDS.clear();
    listenerLoopExecuteMethod();
    CHECK(state.method_calls == 0 && state.command_calls == 0);
    finish("control_empty_queue", ok, before);
}

int main() {
    gCall4D = (Call4DProcPtr)synthetic_call4d;
    control_set_get_method_roundtrip();
    control_empty_queue();
    control_method_no_result();
    control_method_error_not_cleared();
    defect_execute_method_fallback_no_leak();
    defect_method_result_cleared();
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
