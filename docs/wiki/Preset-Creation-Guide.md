# 1. Quick Start (TL;DR)

1. Create a folder for your mod:  
   Data/SKSE/Plugins/DAF/animData/MyMod/
2. Put a JSON file inside (example: MyFile.json)
3. Minimal example:

```json
{
  "priority": 10,
  "events": [5],
  "animations": ["MyPickupIdle"],
  "forms": ["IronSword"]
}
```

Meaning:  
- events: [5] → ItemPickup  
- forms: ["IronSword"] → the item (ID) you’re targeting  
- animations: ["MyPickupIdle"] → animation event name to send (see Important note below)  
- priority: lower numbers override higher numbers

Launch the game → log should show the folder and file were found.

---
> ⚠️⚠️⚠️ Important ⚠️⚠️⚠️  
> Idles (TESIdleForm/IDLE) are currently disabled. Please use animation event names.

---

# 2. Folder Rules

| Rule | Meaning |
|------|---------|
| Base path | Data/SKSE/Plugins/DAF/animData/ |
| Must be inside a subfolder | animData/MyMod/MyFile.json |
| No nested folders | animData/MyMod/Sub/… (ignored) |
| No JSON directly in animData root | Ignored |
| Folder naming | Keep it simple (letters, numbers, underscores) |
| Special characters are skipped | Folders with any of these characters are skipped and warned: `! @ # $ % ^ & * ( ) [ ] { } ; : ' " \ \| , . < > / ?` |

Use one folder per mod/author.

---

# 3. One File = One Definition

Each JSON file describes exactly one animation setup.  
No arrays of multiple definitions. Just plain key/value pairs.

---

# 4. Supported Keys

| Key | Required? | Type | Negation | Form Groups | What It Does |
|-----|-----------|------|----------|-------------|--------------|
| `priority` | No; default `0` | `integer` | No | No | Lower numbers win when multiple presets match |
| `events` | Yes | `integer[]` or `string` | No | No | Uses built-in event numbers `1–28` or one custom event name |
| `animations` | Yes | `string[]` | No | No | Animation events played in order |
| `durations` | No | `integer[]` | No | No | Sets each animation's duration in milliseconds by matching array position |
| `forms` | No | `string[]` | Yes | Yes | Uses IDs to filter the Form supplied by the event |
| `form_types` | No | `integer[]` or `string[]` | Names only | No | Filters by numeric or named Form types; do not mix both types |
| `keywords` | No | `string[]` | Yes | Yes | Uses IDs to filter keywords on the supplied Form |
| `actors` | No | `integer[]` or `string[]` | String IDs only | Yes | Uses IDs to filter the Actor; do not mix both types |
| `locations` | No | `string[]` | Yes | Yes | Uses IDs to filter the Actor's current Location |
| `actor_keywords` | No | `string[]` | Yes | Yes | Uses IDs to filter keywords on the Actor's base NPC |
| `conditions` | No | `string[]` | Yes | Yes | Uses Perk IDs whose conditions are evaluated with the Actor and event Form |
| `attach_node` | No | `string` | No | No | The node in the Animation Object's NIF where the event Form's 3D model is attached |
| `delay` | No | `boolean` or `integer` | No | No | `true` uses total durations; a positive integer sets milliseconds; otherwise no delay |

DAF can delay Activate, ItemAdd, ItemRemove, ItemDrop, ItemPickup, Buy, Sell, and menu-open actions. Other built-in events do not delay their underlying action. Custom events return the configured delay to the plugin that sent the event.

---

# 5. IDs (Very Important)

IDs written as strings can use any of these formats:

1. Plugin-local FormID (load-order safe):  
   `0x01ABCDEF~MyPlugin.esp`

2. Full FormID:  
   `00012EB7`

3. Editor ID:  
   `IronSword`

These values are strings and must be written in quotes in JSON. You may mix these three formats within the same string array.

`actors` also accepts non-negative FormIDs as an integer array. Do not mix integers and strings within the same `actors` array.

Fields marked `Yes` in the `Form Groups` column also accept a [Form Group](https://github.com/QTR-Modding/CLibUtilsQTR/wiki/Form-Groups) name. 
Every Form in the group must have the record type required by that field. For example, an `actors` group must contain only Actor references.

**Form Groups are stored in:**

`Data/SKSE/Plugins/DAF/formGroups`

**Implementation detail:**

The [event table](#6-events-builtin--custom) shows the Actor and Form supplied by each event:

- **Actor:** the Actor reference that plays the animation.
- **Form:** the object related to the event and checked by `forms`, `form_types`, and `keywords`.

If the Form is a placed or runtime reference, those filters check its base record instead. For example, picking up a placed Iron Sword checks the `IronSword` weapon record.

---

# 6. Events (Built‑In & Custom)

You can declare:
- A list of built‑in event numbers, OR
- Use a custom event name when a mod registers/sends its own event via the API with that name.

| Number | Event | Actor | Form |
|--------|-------|-------|------|
| 1 | Activate | Activating actor | Activated reference |
| 2 | ItemAdd | Actor receiving the item | Added item |
| 3 | ItemRemove | Actor losing the item | Removed item |
| 4 | ItemDrop | Actor dropping the item | Dropped item |
| 5 | ItemPickup | Actor picking up the item | Picked-up reference |
| 6 | Equip | Actor equipping the item | Equipped item |
| 7 | Unequip | Actor unequipping the item | Unequipped item |
| 8 | Buy | Buying actor | Bought item |
| 9 | Sell | Selling actor | Sold item |
| 10 | MenuOpenInventory | Player | — |
| 11 | MenuCloseInventory | Player | — |
| 12 | MenuHoverInventory | Item owner | Hovered item |
| 13 | MenuOpenContainer | Player | — |
| 14 | MenuCloseContainer | Player | — |
| 15 | MenuHoverContainer | Item owner | Hovered item |
| 16 | MenuOpenMagic | Player | — |
| 17 | MenuCloseMagic | Player | — |
| 18 | MenuOpenBarter | Player | — |
| 19 | MenuCloseBarter | Player | — |
| 20 | MenuHoverBarter | Item owner | Hovered item |
| 21 | MenuOpenFavorites | Player | — |
| 22 | MenuCloseFavorites | Player | — |
| 23 | MenuOpenMap | Player | — |
| 24 | MenuCloseMap | Player | — |
| 25 | MenuOpenJournal | Player | — |
| 26 | MenuCloseJournal | Player | — |
| 27 | MagicEffectCast | Effect caster | Magic Effect (MGEF) |
| 28 | MagicEffectTarget | Effect target | Magic Effect (MGEF) |

For custom events, the sending plugin supplies the Actor and Form through the [DAF API](https://github.com/QTR-Modding/DynamicAnimationFrameworkSE/wiki/API-Guide).

<u>**Examples**</u>

Built-in events
```json
"events": [5,6]
```

Custom event:
```json
"events": "MyMod_CustomOpen"
```

---

# 7. Animation Chains

Example:
```json
"animations": ["MyIdleStart","MyModAnimSwing","MyIdleLoop"],
"durations":  [0,             1000,              2500]
```

Rules:
- Each duration corresponds to the animation at the same array position.
- If an animation has no matching duration, its duration is `0`.
- `durations` is optional and contains milliseconds.
- Extra durations do not correspond to an animation, but are still included when `"delay": true` calculates the total delay.
- Leave durations out entirely if you don't care about timing.
- IDLE Form support is currently disabled.
- Each animation entry is treated as an animation event name (idles disabled).

Simple:
```json
"animations": ["MyPickupIdle"]
```


---

# 8. Filters (Optional)

If you omit all filters, the preset can match whenever one of its events occurs.

| Filter | Checked Against |
|--------|-----------------|
| `forms` | The Form supplied by the event, or its base record when it is a reference |
| `form_types` | The type of that Form or base record |
| `keywords` | Keywords on that Form or base record |
| `actors` | The Actor reference that plays the animation |
| `actor_keywords` | The Actor's base NPC record |
| `locations` | The Actor's current Location |
| `conditions` | Perk conditions evaluated with the Actor as Subject and the original Form reference as Target, when available |

Matching rules:

- Entries without `!` are includes. Entries beginning with `!` are exclusions.
- Entries within one include filter use OR: any one may match.
- Different filter categories use AND: every configured category must match.
- Any matching exclusion prevents the preset from matching the current event.
- An empty filter array behaves like an omitted filter.
- For `conditions`, at least one included Perk must evaluate true. Any excluded Perk that evaluates true prevents the preset from matching the current event.
- The Actor does not need to have the Perk used by `conditions`.

In the [event table](#6-events-builtin--custom), `—` means the event provides no Form. Therefore, a preset using an include filter for `forms`, `form_types`, or `keywords` cannot match that event. 
Actor-based filters can still match these events.

`actors` must use Actor reference IDs (`ACHR`), not NPC base-record IDs (`NPC_`).

Keywords created at runtime by KID or SPID can be used by Editor ID in `keywords` and `actor_keywords`.

> ⚠️ If an included or excluded ID cannot be resolved, a referenced Form Group is empty, or a form type or event number is invalid, DAF skips the entire preset file. The log identifies the problem and file.

# 9. Priority (Lower Number = Stronger)

DAF sorts matches by ascending priority and picks the first.  
Lower = overrides higher.

Avoid using the same priority for definitions that might clash.

---

# 10. Examples

Minimal:
```json
{
  "priority": 10,
  "events": [5],
  "animations": ["MyPickupIdle"],
  "forms": ["IronSword"]
}
```

Multiple events:
```json
{
  "priority": 25,
  "events": [6,7],
  "animations": ["MyMod_Draw","MyMod_Settle"],
  "durations": [20,40],
  "keywords": ["WeapTypeSword"]
}
```

Custom event:
```json
{
  "priority": 5,
  "events": "MyMod_Inspect",
  "animations": ["MyMod_InspectPose"]
}
```

Negation:
```json
{
  "priority": 12,
  "events": [5],
  "animations": ["MyPickupEvent"],
  "keywords": ["WeapTypeSword", "!WeapTypeDagger"],
  "forms": ["!Gold001"],
  "form_types": ["Weapon", "!Armor"],
  "actors": ["!PlayerRef"],
  "locations": ["!SovngardeLocation"]
}
```

Conditions:
```json
{
  "priority": 8,
  "events": [6],
  "animations": ["MyMod_Draw"],
  "conditions": ["Armsman00", "!Stealth00"]
}
```

Layered naming strategy:
```
animData/MyMod/
  05_ItemSpecific.json
  20_WeaponType.json
  60_GenericFallback.json
```

---

# 11. Naming Tips

| Thing | Tip | Example |
|-------|-----|---------|
| Folder | Use mod or author name | MyMod |
| File | `<priority>_<topic>.json` | 05_IronSword.json |
| Custom event | Prefix with mod name | MyMod_Inspect |
| Animation names | Consistent prefix | MyMod_Draw |

---

# 12. Troubleshooting

| Problem | Why | Fix |
|---------|-----|-----|
| File ignored | Wrong folder level | Place in animData/MyMod/ |
| Folder skipped | Special characters in folder name | Use letters, numbers, underscores only |
| Not listed in log | Wrong extension | Must be .json |
| Animation not playing | Filters too strict / wrong event | Temporarily remove filters & test |
| ID not found | Typo or format issue | Re-check IDs |
| Wrong definition chosen | Priority misunderstanding | Lower number wins |
| delay true returns 0 | All durations are 0 | Use a number or add durations |
| Custom event unused | Not triggered | Other mod must send it |
| Nothing happens on pickup/activate | Target ref disabled/deleted | Ensure the reference is valid/alive |

---

# 13. Pre-Test Checklist

- [ ] File is in animData/MyMod/ (one folder deep)
- [ ] Folder name contains only letters/numbers/underscores (no special chars)
- [ ] events set (numbers or one custom name)
- [ ] animations list not empty
- [ ] durations (if present) are in milliseconds and correspond to animations by array position
- [ ] IDs valid where used
- [ ] priority chosen (lower = stronger)
- [ ] delay correct (or omitted)
- [ ] Only ONE custom event string if using custom event

---

# 14. Starter Templates

Standard:
```json
{
  "priority": 20,
  "events": [EVENT_NUMBER],
  "animations": ["YourAnimEventName"],
  "forms": ["IronSword"]
}
```

Custom event:
```json
{
  "priority": 10,
  "events": "MyMod_CustomEvent",
  "animations": ["MyMod_CustomAnim"]
}
```

---

Happy animating!