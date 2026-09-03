# Dynamic Animation Framework SE
Dynamic Animation Framework SE (DAF) lets you trigger custom animation chains in Skyrim Special Edition when certain in-game or menu events happen (pick up, drop, equip, activate, open inventory, etc.).  
You add simple JSON files that say: “When event X happens, and the item / actor matches these filters, play these animations in this order.”

Animations can be behavior animation-event names or `TESIdleForm` (`IDLE`) records. IDLE roots/containers can resolve to a valid playable child at runtime, and when an event supplies a reference DAF passes it as the IDLE target, so paired IDLEs can work without a separate JSON flag.

Variable files can calculate animation durations, read and write animation graph variables, and evaluate condition functions immediately before playback. Individual variable definitions can optionally swap Subject and Target so they operate on the event reference instead of the animation actor.

Other mods also have the option to register their custom events with DAF.

No Papyrus or coding required.
