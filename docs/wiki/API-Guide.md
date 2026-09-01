# Dynamic Animation Framework SE – Native C++ API Reference

This page explains the [C++ API header](https://github.com/QTR-Modding/DynamicAnimationFrameworkSE_API) you can include in your own SKSE plugin to send events to DAF.<br>
Refer to the README of the linked repository for instructions on how to include it in your project automatically.<br>
It lets you:
- Retrieve the numeric ID for your custom animation event.
- Send that event to DAF so it can evaluate JSON definitions and play the corresponding animation chain.

---

## Header Overview

Provided header (simplified):

```cpp
#pragma once
#include <windows.h>
#include <cstdint>

namespace DAF_API {

    using AnimEventID = uint32_t;

    // RequestEventID(...) -> returns numeric event ID (0 if not found / incompatible)
    // SendEvent(...)      -> triggers the event (returns 0 on failure)
}
```

Two public functions you actually use:

| Function | Purpose |
|----------|---------|
| `AnimEventID RequestEventID(const char* name)` | Resolve a custom event name to its numeric ID (returns 0 if unavailable or version mismatch). |
| `int SendEvent(AnimEventID id, uint32_t actorFormID, uint32_t itemFormID)` | Dispatch an event to DAF for (optional) the given actor and (optional) related item/form. If it matches a JSON preset, it returns the animation duration specified in the matched animation data. |

---

## How Event IDs Exist

1. Built‑in events (e.g., Equip, ItemPickup) have fixed IDs defined internally by DAF.
2. Custom events are registered automatically when a JSON definition uses a single string in `"events": "YourCustomEventName"`.
3. Once DAF has loaded the JSON files, `RequestEventID("YourCustomEventName")` will resolve to a non‑zero `AnimEventID`.

Important: You do NOT “create” a custom event from code. It is declared in JSON first, then looked up by name from code.

---

## Function Details

### `AnimEventID RequestEventID(const char* name)`

Resolves a name → numeric ID.

Return:
- `> 0`: Valid ID (cache it).
- `0`: Not found or DAF not loaded or version mismatch.

When to call:
- After DAF is guaranteed to be loaded (e.g., SKSE Messaging Interface “DataLoaded” or later).
- Lazily on first use, then store the result.

### `int SendEvent(AnimEventID id, uint32_t actorFormID, uint32_t itemFormID)`

Dispatches an event:
- `id`: Must be a non‑zero valid `AnimEventID`.
- `actorFormID`: The reference ID of the actor you want to target. Can be nullptr if not relevant.
- `itemFormID`: The associated item form ID (can be 0 if not relevant to your event).

Return:
- The total duration of the animation chain specified by the matched JSON file, in case there is a match.

---

## Typical Workflow (Custom Event Example)

1. In your mod’s JSON:
   ```json
   {
     "priority": 5,
     "events": "MyMod_Inspect",
     "animations": ["MyMod_InspectPose"]
   }
   ```
   DAF loads this and registers `"MyMod_Inspect"`.

2. In your C++ plugin (after DataLoaded):
   ```cpp
   #include "DAF_API.h" // (your copy of the header)
   using namespace DAF_API;

   static AnimEventID g_inspectEvent = 0;

   void ResolveDAFEvents() {
       if (g_inspectEvent == 0) {
           g_inspectEvent = RequestEventID("MyMod_Inspect");
           if (g_inspectEvent == 0) {
               // Log: Failed (maybe JSON not loaded yet or DAF missing)
           }
       }
   }

   void TriggerInspect(RE::Actor* actor, RE::TESForm* relatedItem) {
       if (!actor) return;
       ResolveDAFEvents();
       if (g_inspectEvent) {
           uint32_t actorID = actor->GetFormID();
           uint32_t itemID  = relatedItem ? relatedItem->GetFormID() : 0;
           auto duration = SendEvent(g_inspectEvent, actorID, itemID);
       }
   }
   ```

3. Player triggers something in your mod → you call `TriggerInspect(...)` → DAF evaluates matches and plays chain if defined.

---

## Built‑In Event Example (Equip)

```cpp

void OnEquipped(RE::Actor* actor, RE::TESForm* item) {
    if (!actor|| !item) return;
    static AnimEventID equipEvent = 6;
    DAF_API::SendEvent(equipEvent, actor->GetFormID(), item->GetFormID());
}
```

---

## Error Handling & Defensive Checks

| Scenario | Symptom | Your Action |
|----------|---------|-------------|
| DAF not installed | `RequestEventID` returns 0 | Skip calls; optionally log once |
| JSON not loaded yet | Early call returns 0 | Delay until SKSE “DataLoaded” message |
| Wrong event name | Always 0 | Verify spelling / case in JSON & code |
| Version mismatch | 0 even for known name | Ensure header MAJOR/MINOR matches DAF build |

Tip: Cache success; avoid spamming lookups every frame.

---

## Best Practices

- Prefix custom event names with your mod name to avoid collisions (e.g., `MyMod_`).
- Store resolved IDs in static/globals after first successful lookup.
- Resolve events after all JSON packs are loaded (post DataLoaded).

---

## FAQ

Q: Do I need to “register” a custom event from code?  
A: No. You define it in a JSON file (single string under `"events"`). DAF registers it during load. Then you look it up by name.

Q: Can one JSON definition list multiple custom event names?  
A: No. Custom (string) form supports a single name. Use multiple JSON files if you need different names pointing to the same animation chain.

Q: What if I call `SendEvent` with itemFormID = 0? What about actors?  
A: That is allowed; DAF will simply consider item filters that do not rely on a specific item form (or they will fail if they require one).
Same for actors.

Q: Are IDs stable across installs?
A: Not guaranteed for custom events; always resolve by name at runtime (don’t serialize the numeric ID into save data expecting permanence).

Q: Is the call synchronous?  
A: Lookup and dispatch are immediate; animation evaluation happens inside DAF promptly. There is no callback; you just fire and let DAF handle it.

---

## Quick Checklist

| Task | Done? |
|------|-------|
| Added header to project | |
| Wait for DAF loaded (DataLoaded) before resolving events | |
| Created JSON with custom event names | |
| Called `RequestEventID` once & cached result | |

---