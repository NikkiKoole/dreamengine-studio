#ifndef TINYJAM_STORE_H
#define TINYJAM_STORE_H
#include <stdbool.h>

// The ONLY file the C cart needs to know about purchases. Implemented in Swift
// (Store.swift, via @_cdecl) over StoreKit 2 — no backend, on-device verification.
// Per product-notes-followup.md §7 (in-house, no third-party SDK).
void Store_Init(void);                              // load products + current entitlements
bool Store_IsModuleUnlocked(const char* moduleId);  // gate a rack (masterpass unlocks all)
void Store_Purchase(const char* moduleId);          // fire the App Store purchase sheet
bool Store_TestingAvailable(void);                  // DEBUG only — true when SKTestSession is active (gates the reset button)
void Store_ResetPurchases(void);                    // DEBUG only — wipe local test purchases (SKTestSession)
#endif
