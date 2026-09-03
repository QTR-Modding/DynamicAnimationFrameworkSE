# Variables

DAF variable files can calculate helpers and animation durations, read existing
animation graph variables, and set graph variables immediately before an
animation plays.

The animation event and any graph variable DAF reads or sets must already exist
in your behavior files. Their names and types must match. Restart Skyrim after
editing DAF configs.

## Quick start

The example names `TakeItemCustom` and `II_AnimationSpeed` come from
Monitor221hz's [Animated Interactions SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/143798).

Create one animation config and one variable file:

```text
Data/SKSE/Plugins/DAF/
  animData/MyMod/pickup.json
  varData/MyMod/take.json
```

In `pickup.json`, add `variables`:

```json
{
  "events": [5],
  "animations": ["TakeItemCustom"],
  "variables": ["take"]
}
```

Event `5` is item pickup. `variables[0]` belongs to `animations[0]`. Here,
`take` means `varData/MyMod/take.json`.

Put this in `take.json`:

```json
{
  "II_AnimationSpeed": {
    "value": 1.5,
    "type": 2
  }
}
```

Before `TakeItemCustom` plays, DAF sets the float graph variable
`II_AnimationSpeed` to `1.5`.

`type` tells DAF what to write:

| `type` | Writes |
| ---: | --- |
| `0` | Boolean: `0` is false; any other valid number is true |
| `1` | Integer: removes the decimal part |
| `2` | Float |

## Match files to animations

The arrays match by position:

```json
{
  "animations": ["Take", "TakeLow", "TakeHigh"],
  "variables": ["take", "", "takeHigh"]
}
```

- `Take` uses `take.json`.
- `TakeLow` uses no variable file.
- `TakeHigh` uses `takeHigh.json`.

`variables` may be omitted, empty, or shorter than `animations`. Missing
trailing entries use no variable file. Use `""` to skip an earlier animation.

Write only the file name, without `.json`. Folder names must match:
`animData/MyMod` uses `varData/MyMod`.

## Graph variables and helpers

`take.json` can contain both helpers and graph variables to set:

```json
{
  "baseSpeed": 1.5,
  "durationMs": 3200,

  "II_AnimationSpeed": {
    "value": "baseSpeed",
    "type": 2
  },

  "bTakeCustomMirror": {
    "value": true,
    "type": 0
  }
}
```

- `baseSpeed` and `durationMs` are helpers. DAF does not write them to the graph.
- A definition with `type` is written to the graph. Its key must be the exact
  graph variable name.
- A definition may use only definitions written above it.

DAF starts with graph variables and named durations. It calculates referenced
helpers recursively and ignores unused helpers. A helper is recalculated every
time it is used.

These are all supported fields:

| Field | Meaning |
| --- | --- |
| `value` | Value to use. Required in object form. |
| `type` | Graph type: `0` Boolean, `1` Integer, `2` Float. Without it, this is a helper. |
| `target` | Boolean; default `false`. When `true`, swaps Subject and Target for this definition. |
| `conditions` | Earlier definitions or Perks that choose between `value` and `else`. |
| `else` | Used when `conditions` fail. Default: `0`. |
| `post` | Changes `value` after it is found. |

A bare Boolean or number, such as `"baseSpeed": 1.5`, is helper shorthand.

## Evaluate a definition against the event Target

Normally, every definition uses:

```text
Subject = animation Actor
Target  = event reference, when the event supplies one
```

Set:

```json
"target": true
```

on an expanded definition to swap those roles for that definition:

```text
Subject = event reference
Target  = animation Actor
```

Example:

```json
{
  "targetScale": {
    "value": [24],
    "target": true
  }
}
```

`[24]` is `GetScale()`. With `target: true`, it reads the event Target's scale
instead of the animation Actor's scale.

The swap applies to that definition's:

- condition-function/provider calls,
- Perk conditions,
- animation graph reads, and
- animation graph writes.

For a graph output, `target: true` therefore writes the graph variable to the
event Target's animation graph instead of the animation Actor's graph:

```json
{
  "II_AnimationSpeed": {
    "value": 2.0,
    "type": 2,
    "target": true
  }
}
```

The event Target must exist for a swapped definition. Graph reads/writes also
require that the effective Subject has the requested animation graph variable.
If evaluation or a graph operation fails, DAF skips that animation.

`target` is **per definition**. If one definition references another definition,
the referenced definition uses its own `target` setting; it does not inherit the
caller's setting.

Literal values and Globals do not themselves depend on Subject/Target, but any
conditions, provider calls, graph reads, or graph writes in the definition still
use the definition's selected context.

## Calculate a duration

`durations` accepts static milliseconds and definition names together:

```json
{
  "animations": ["Take", "TakeHigh"],
  "variables": ["take", "takeHigh"],
  "durations": ["durationMs", 3200]
}
```

`durationMs` is the helper defined in `take.json` above. It is evaluated
immediately before `Take` plays. It can use everything described below,
including conditions, globals, condition functions, graph reads, `target`, and
`post`.

The result is rounded to the nearest millisecond.

An omitted duration, an empty definition name, or no variable file mapped to
that animation uses `0`. A missing definition in a mapped file rejects the config.
If an existing definition cannot be calculated, is negative, or is too large,
that animation is skipped.

`delay: true` can total only durations written directly as numbers. If any
duration is calculated from a variable file, set `delay` to a number instead.

## Choose a value

`value` and `else` accept:

| Example | Uses |
| --- | --- |
| `true` or `1.5` | That exact value |
| `"baseSpeed"` | An earlier definition |
| `"0x802~MyMod.esp"` | A Global value |
| `["II_AnimationSpeed", 2]` | An [animation graph variable value](#read-a-graph-variable) |
| `[24]` | `GetScale()` result; see [condition functions](#use-a-condition-function-advanced) |

A string matching an earlier definition uses that definition. Otherwise, DAF
resolves it as a [plugin-local ID, full FormID, or EditorID](https://github.com/QTR-Modding/DynamicAnimationFrameworkSE/wiki/Preset-Creation-Guide#5-ids-very-important).
Here it must resolve to a Global (`TESGlobal`).

DAF calculates every value as a number. When it writes a graph variable, `type`
decides whether it writes a Boolean, Integer, or Float. If the result cannot be
calculated or written, DAF skips the animation.

## Use conditions and `else`

Each condition can be an earlier definition or a Perk Form identifier:

```json
{
  "enabled": {
    "value": "0x801~MyMod.esp",
    "post": {
      "gt": 0
    }
  },

  "II_AnimationSpeed": {
    "value": 1.5,
    "conditions": ["enabled", "0x800~MyMod.esp"],
    "post": {
      "multiply": 2
    },
    "else": 1.0,
    "type": 2
  }
}
```

- If any listed definition is nonzero or any listed Perk passes, DAF uses
  `value`. Values approximately equal to zero are false.
- If none pass, DAF uses `else`. Its default is `0`.
- With no `conditions`, or an empty list, DAF always uses `value`.
- `post` changes only `value`, not `else`.

Referenced definitions are calculated on demand and must be written earlier in
the same file. If one cannot be calculated, DAF skips the animation.

Perk conditions normally receive the animation Actor as Subject and the event
reference as Target when the event has one. On a definition with `target: true`,
those two roles are swapped.

To apply `post` to both paths, make the choice in a helper:

```json
{
  "chosenSpeed": {
    "value": 2.0,
    "conditions": ["0x800~MyMod.esp"],
    "else": 1.0
  },

  "II_AnimationSpeed": {
    "value": "chosenSpeed",
    "post": {
      "clamp": [0.5, 3.0]
    },
    "type": 2
  }
}
```

## Change a value with `post`

Operations run from top to bottom:

| Operation | Example |
| --- | --- |
| Add | `"add": 1` |
| Subtract | `"subtract": 1` |
| Multiply | `"multiply": -1` |
| Divide | `"divide": 2` |
| Power | `"pow": 2` |
| Clamp | `"clamp": [0, 1]` |
| Absolute value | `"abs": true` |
| Floor | `"floor": true` |
| Ceiling | `"ceil": true` |
| Round decimal places | `"round": 2` |
| Upper bound | `"min": 1` |
| Lower bound | `"max": 0` |
| Natural logarithm | `"log": true` |
| Exponential | `"exp": true` |
| Sine | `"sin": true` |
| Cosine | `"cos": true` |
| Tangent | `"tan": true` |
| Arcsine | `"asin": true` |
| Arccosine | `"acos": true` |
| Arctangent | `"atan": true` |
| Two-argument arctangent | `"atan2": 1` |
| Less than | `"lt": 1` |
| Less than or equal | `"le": 1` |
| Greater than | `"gt": 1` |
| Greater than or equal | `"ge": 1` |
| Equal | `"eq": 1` |
| Not equal | `"ne": 1` |

Numbers may be replaced with the name of an earlier definition. Clamp accepts
two numbers or names. Operations without an operand require `true`.

`round` takes the number of decimal places. The number is truncated toward
zero before use. `2` rounds `12.3456` to `12.35`, `0` rounds it to `12`, and
`-1` rounds it to `10`. Halfway values round away from zero.

`min` keeps the lower of the current value and its operand. `max` keeps the
higher. `atan2` calculates `atan2(current, operand)`, where the current value is
Y and the operand is X. `atan2(0, 0)` gives `0`.

Comparisons return `1` or `0`. DAF automatically treats tiny rounding
differences as equal. Equal values are excluded by `lt` and `gt`, and included by
`le` and `ge`.

Division by zero gives `0`. `log` is the natural logarithm and requires a
positive value. Trigonometric operations use radians. `asin` and `acos` accept
`-1` through `1`. A reversed clamp or any operation that does not produce a
valid number skips the animation.

## Read a graph variable

Use `[name, type]`:

```json
{
  "II_AnimationSpeed": {
    "value": ["II_AnimationSpeed", 2],
    "post": {
      "divide": 2
    },
    "type": 2
  }
}
```

This reads the current float value, halves it, and writes it back.

- `["name", type]` reads an existing graph variable.
- Every graph read happens before this file sets any graph variables.
- A definition with `target: true` reads from the event Target's graph instead
  of the animation Actor's graph.

Use the same type numbers as above. The type must match the graph variable.

DAF does not reset graph variables after an animation. A later animation can
read an earlier value if the behavior or another mod has not changed it.

## Use a condition function (advanced)

Use `[functionID, arguments...]` to get a condition-function result:

```json
{
  "scale": {
    "value": [24]
  }
}
```

The first number is the function ID. Find Skyrim function IDs in CommonLib's
[FunctionID list](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/4190ec291f99c64b765c0647e25cf8a3a3d9a550/include/RE/T/TESCondition.h#L33).
For example, `kGetScale = 24`. The
[Creation Kit list](https://ck.uesp.net/wiki/Condition_Functions) explains what
each function does and which parameters it takes.

[CommunityFunctionsSE](https://github.com/QTR-Modding/CommunityFunctionsSE)
lists community function IDs. Its range is `1000` through `9999`.

Normally DAF supplies the animation Actor as Subject. Target is the event
reference, such as the item being picked up, when the event supplies one.

On a definition with `target: true`, DAF swaps those roles: the event reference
is Subject and the animation Actor is Target.

Events that remove their reference, such as item pickup, can lose Target before
the animation runs. If a condition function needs it, use a numeric `delay` in
the animation config.

Arguments work like this:

- If a Skyrim function's first argument is listed as `ObjectRef`, DAF uses Target
  automatically when Target exists. Do not include that argument in the array.
- If the event has no Target and the function requires that argument, provide
  the Form or reference yourself.
- Community functions receive Target in each argument registered as `Target`.
- Write the remaining arguments after the function ID, in the order documented
  for that function.
- Write Forms and references using
  [Form identifier strings](https://github.com/QTR-Modding/DynamicAnimationFrameworkSE/wiki/Preset-Creation-Guide#5-ids-very-important).
- Write numeric arguments and numbered choices such as Axis as JSON numbers.
- Write arguments directly. Helpers and graph reads cannot be used here.

Inside a condition-function array, a Form identifier passes the Form itself. It
does not read a Global's value. Missing, extra, or incompatible arguments make
the calculation fail, so DAF skips the animation. If a function marks its final
arguments as optional, you may leave them out.

Examples:

| Function | `value` | Meaning |
| --- | --- | --- |
| `GetScale()` | `[24]` | Subject's scale |
| `GetDistance(ObjectRef)` | `[1]` | Distance to Target; requires Target |
| `GetPos(Axis)` | `[6, 88]` | Subject's X position; `89` is Y, `90` is Z |
| `GetStageDone(Quest, Stage)` | `[59, "MyQuestEditorID", 20]` | Whether stage 20 is done |
| `GetWithinDistance(ObjectRef, Float)` | `[639, 256]` | Whether Target is within 256 units |
| `WouldBeStealing(ObjectRef)` | `[1000]` | Whether taking Target would be stealing |

Supported functions and arguments:

- Skyrim function IDs are `0` through `735`. Registered CommunityFunctionsSE IDs
  are `1000` through `9999`.
- IDs `736` through `999` are reserved.
- DAF can use a function only when Skyrim or CommunityFunctionsSE exposes it as a
  condition and its arguments are supported below.
- Functions that list more than two arguments are not supported. Subject is
  separate and does not count.
- Arguments other than Forms, references, and numbers are not supported.
- Skyrim whole-number arguments and numbered choices such as Axis cannot use
  decimals. For example, write `88`, not `88.5`.
- `GetWithinDistance` is the only supported Skyrim function with a `Float`
  argument. Its distance must still be a whole number of `0` or more.
- Community integer arguments must be whole numbers. Community float arguments
  may use decimals.
- DAF rejects numbers that are too large for the function to use.

If a condition function cannot return a value, DAF skips the animation. It does
not use `else`; `else` is only for failed `conditions`.

## When something goes wrong

- If a named variable file is missing or invalid, its animation config does not
  load.
- DAF calculates a named duration and every graph variable value before writing.
  If a calculation fails, nothing changes and the animation is skipped.
- A `target: true` definition fails if the event Target is unavailable.
- A swapped graph read/write fails if the event Target does not have the
  requested animation graph variable.
- DAF then writes in file order. If a later write fails, earlier changes remain.
  The animation is still skipped.
- The next queued animation still runs.

Check the DAF log for the reason.
