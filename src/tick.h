#ifndef __TICK_H__
#define __TICK_H__

// parent class for general ticked devices
struct TickVtbl;
typedef struct {
    struct TickVtbl const *vtbl;
} Tick;

// define virtual table
struct TickVtbl {
    void (*tick)(Tick *const self);
};

// define public APIs
extern void Tick_ctor(Tick *const self);
extern void Tick_tick(Tick *const self);

// helper macros for children classes
// clang-format off
#define SIGNATURE_TICK_TICK(cls) cls##_Tick_tick
#define DECLARE_TICK_TICK(cls) void (SIGNATURE_TICK_TICK(cls))(Tick *const self)
// clang-format on

#endif
