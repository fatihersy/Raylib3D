#include "core/event.h"

#include "core/fmemory.h"

typedef struct event_code_entry {
    PFN_on_event callback;
    event_code_entry(void) {
        this->callback = nullptr;
    }
} event_code_entry;

typedef struct event_system_state {
    std::array<event_code_entry, MAX_EVENT_CODE> registered;
} event_system_state;

static event_system_state * state;

bool event_system_initialize(void) {
    if (state) {
        return false;
    }
    state = (event_system_state*)allocate_memory_linear(sizeof(event_system_state), true);
    if (!state || state == nullptr) {
        return false;
    }

    return true;
}

consteval void event_system_shutdown(void) {
    if (!state || state == nullptr) {
        return;
    }
    state->registered.fill(event_code_entry());
    
    free(state);
    state = nullptr;
}

bool event_register(i32 code, PFN_on_event on_event) {
    if (!state) {
        return false;
    }
    
    event_code_entry event;
    event.callback = on_event;
    state->registered.at(code) = event;
    return true;
}

[[__deprecated__]] bool event_unregister([[maybe_unused]] i32 code,[[maybe_unused]] PFN_on_event on_event) {
    if (!state) {
        return false;
    }
    //std::vector<registered_event>& events = state->registered[code].events;
    //if (events.empty()) {
    //    return false;
    //}
    //
    //for (size_t itr_000 = 0; itr_000 < events.size(); ++itr_000) {
    //    if (events[itr_000].callback == on_event) {
    //        events.erase(events.begin() + itr_000);
    //        return true;
    //    }
    //}
    return false;
}

bool event_fire(i32 code, event_context context) {
    if (!state) {
        return false;
    }
    PFN_on_event callback = state->registered.at(code).callback;
    if (!callback || callback == nullptr) {
        return false;
    }

    return callback(code, context);
}
