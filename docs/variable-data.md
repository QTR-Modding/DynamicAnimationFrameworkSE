# Animation graph variables

DAF can set animation graph variables immediately before an animation plays.

The animation event and graph variables must already exist in your behavior
files. Their names and types must match. Restart Skyrim after editing DAF
configs.

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
trailing entries use no variable file. Use `""` only to skip an animation
before a later mapping. More entries than animations reject the config.

Write only the file name, without `.json`. Folder names must match:
`animData/MyMod` uses `varData/MyMod`.

## Calculate a duration

`durations` accepts static milliseconds and definition names together:

```json
{
  "animations": ["Take", "TakeHigh"],
  "variables": ["take", "takeHigh"],
  "durations": ["durationMs", 3200]
}
```

`durationMs` is evaluated from `take.json` immediately before `Take` plays.
It can use everything described below, including conditions, globals,
condition functions, graph reads, and `post`. Its decimal part is discarded.

An omitted duration, an empty definition name, or no variable file mapped to
that animation uses `0`. A missing definition also uses `0` and logs a warning.
If an existing definition cannot be calculated or is outside the supported
millisecond range, that animation is skipped.

`delay: true` totals only static durations. Use a numeric `delay` when durations
are calculated at play time.

## Write a variable file

Each key is either a helper or a graph variable to set:

```json
{
  "baseSpeed": 1.5,

  "II_AnimationSpeed": {
    "value": "baseSpeed",
    "post": {
      "multiply": 2
    },
    "type": 2
  },

  "bTakeCustomMirror": {
    "value": true,
    "type": 0
  }
}
```

- `baseSpeed` is a helper. DAF does not write it to the graph.
- A definition with `type` is written to the graph. Its key must be the exact
  graph variable name.
- A definition may use only definitions written above it.

A helper is recalculated every time it is used.

These are all supported fields:

| Field | Meaning |
| --- | --- |
| `value` | Value to use. Required. |
| `type` | Graph type: `0` Boolean, `1` Integer, `2` Float. Without it, this is a helper. |
| `conditions` | Perks whose conditions choose between `value` and `else`. |
| `else` | Used when `conditions` fail. Default: `0`. |
| `post` | Changes `value` after it is found. |

A bare Boolean or number, such as `"baseSpeed": 1.5`, is helper shorthand.

## Choose a value

`value` and `else` accept:

| Example | Uses |
| --- | --- |
| `true` or `1.5` | That exact value |
| `"baseSpeed"` | An earlier definition |
| `"0x802~MyMod.esp"` | A `TESGlobal` value |
| `["II_AnimationSpeed", 2]` | A graph variable value |
| `[24]` | `GetScale()` result; see [condition functions](#use-a-condition-function-advanced) |

A string matching an earlier definition uses that definition. Otherwise, DAF
resolves it as a [plugin-local ID, full FormID, or EditorID](https://github.com/QTR-Modding/DynamicAnimationFrameworkSE/wiki/Preset-Creation-Guide#5-ids-very-important).
Here it must resolve to a `TESGlobal`.

DAF works with floats until it writes a graph variable. It then applies its
declared `type`. At play time, an invalid result or an integer outside the
signed 32-bit range skips the animation.

## Use conditions and `else`

Put your Creation Kit conditions in a `BGSPerk`'s top-level condition list,
then add its Form identifier:

```json
{
  "II_AnimationSpeed": {
    "value": 1.5,
    "conditions": ["0x800~MyMod.esp"],
    "post": {
      "multiply": 2
    },
    "else": 1.0,
    "type": 2
  }
}
```

- If any listed perk passes, DAF uses `value`.
- If none pass, DAF uses `else`. Its default is `0`.
- With no `conditions`, or an empty list, DAF always uses `value`.
- `post` changes only `value`, not `else`.

Perk conditions receive the animation actor as Subject. They receive the event
reference as Target when the event has one.

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
| Arcsine | `"asin": true` |

Numbers may be replaced with the name of an earlier definition. Clamp accepts
two numbers or names.

Division by zero gives `0`. `asin` accepts `-1` through `1` and returns radians.
A reversed clamp or any operation that does not produce a valid number skips
the animation.

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

Use the same type numbers as above. The type must match the graph variable.

DAF does not reset graph variables after an animation. A later animation can
read an earlier value if the behavior or another mod has not changed it.

## Use a condition function (advanced)

Use `[functionID, arguments...]` to get a vanilla condition-function result:

```json
{
  "scale": {
    "value": [24]
  }
}
```

The first number is the function ID. Find it in CommonLib's
[`RE::FUNCTION_DATA::FunctionID` enum](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/4190ec291f99c64b765c0647e25cf8a3a3d9a550/include/RE/T/TESCondition.h#L33).
For example, `kGetScale = 24`. The
[Creation Kit list](https://ck.uesp.net/wiki/Condition_Functions) explains what
each function does and which parameters it takes.

DAF always supplies the animation actor as Subject. Target is the event
reference, when one exists.

Arguments work like this:

- If the first function parameter is exactly `ObjectRef` and Target exists,
  DAF always uses Target. A config argument cannot replace it.
- Config arguments fill the remaining function parameters in order.
- DAF never supplies the second function parameter.
- Forms and references use Form identifier strings. Numbers and enums use JSON numbers.
- Arguments must be literal values. They cannot use helpers or graph reads.

Inside this array, a Form identifier string passes the Form itself. It does not
read a `TESGlobal` value. Missing, extra, or wrong arguments reject the call.
Optional trailing parameters may be omitted.

Examples:

| Function | `value` | Meaning |
| --- | --- | --- |
| `GetScale()` | `[24]` | Subject's scale |
| `GetDistance(ObjectRef)` | `[1]` | Distance to Target; requires Target |
| `GetPos(Axis)` | `[6, 88]` | Subject's X position; `89` is Y, `90` is Z |
| `GetWithinDistance(ObjectRef, Float)` | `[639, 256]` | Whether Target is within 256 units |

Current limits:

- Only vanilla IDs `0` through `735` are available.
- Functions with more than two parameters are not supported.
- Numeric parameters are supported only for `GetPos` (`6`) and
  `GetWithinDistance` (`639`).
- `GetWithinDistance` needs a whole, non-negative distance within the 32-bit
  unsigned integer range.

If a condition function cannot return a value, DAF skips the animation. It does
not use `else`; `else` is only for failed `conditions`.

## When something goes wrong

- If a named variable file is missing or invalid, its animation config does not
  load.
- DAF gets every graph variable value before writing. If that fails, nothing
  changes and the animation is skipped.
- DAF then writes in file order. If a later write fails, earlier changes remain.
  The animation is still skipped.
- The next queued animation still runs.

Check the DAF log for the reason.
