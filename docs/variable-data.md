# Animation graph variable data

DAF can calculate and set animation graph variables immediately before an
animation plays. Each animation may use its own variable file.

DAF does not add animations, animation events, or variables to a behavior
graph. They must already exist, and every graph variable name and type in these
files must match the behavior exactly. DAF loads its configuration at startup,
so restart Skyrim after editing it.

## Quick start

This example assumes the behavior already contains the animation event
`TakeItemCustom` and the float variable `II_AnimationSpeed`. Those names come
from Animated Interactions; replace them with names provided by your behavior.

Create these files:

```text
Data/SKSE/Plugins/DAF/
  animData/
    MyMod/
      pickup.json
  varData/
    MyMod/
      take.json
```

Add `variables` to the animation config:

```json
{
  "events": [5],
  "animations": ["TakeItemCustom"],
  "variables": ["take"]
}
```

In this example, DAF event `5` is item pickup. `variables[0]` belongs to
`animations[0]`, so `take` selects `varData/MyMod/take.json`.

Put this in `take.json`:

```json
{
  "II_AnimationSpeed": {
    "value": 1.5,
    "type": 2
  }
}
```

Before DAF sends `TakeItemCustom`, it sets the float graph variable
`II_AnimationSpeed` to `1.5`.

The graph types are:

| `type` | Graph variable type |
| ---: | --- |
| `0` | Boolean |
| `1` | Integer |
| `2` | Float |

## Matching animations to variable files

The `animations` and `variables` arrays are matched by position:

```json
{
  "animations": ["Take", "TakeLow", "TakeHigh"],
  "variables": ["take", "", "takeHigh"]
}
```

This means:

- `Take` uses `take.json`.
- `TakeLow` uses no variable file.
- `TakeHigh` uses `takeHigh.json`.

Rules:

- `variables` is optional. A non-empty array must have exactly one entry per
  animation.
- Use `""` when an animation does not need variable data.
- Enter only the file name, without a path or `.json`.
- The folder name must match: a config under `animData/MyMod` can use only files
  under `varData/MyMod`.
- DAF does not reset graph variables for entries without a variable file.

## Writing a variable file

A variable file is one JSON object. Each top-level key defines a value:

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
    "value": false,
    "type": 0
  }
}
```

Here:

- `baseSpeed` is a helper because it has no `type`.
- `II_AnimationSpeed` and `bTakeCustomMirror` are written to the graph because
  they have `type`.
- The top-level key is the exact graph variable name whenever `type` is present.

An expanded definition supports exactly these fields:

| Field | Required | Purpose |
| --- | --- | --- |
| `value` | Yes | Supplies the value to calculate. |
| `type` | No | Writes this definition to the graph as Boolean (`0`), Integer (`1`), or Float (`2`). Without it, the definition is a helper. |
| `conditions` | No | Lists `BGSPerk` records whose conditions decide whether `value` is used. |
| `else` | No | Supplies a value when `conditions` do not pass. Defaults to `0`. |
| `post` | No | Changes the calculated `value`. |

A bare Boolean or number is shorthand for a helper:

```json
{
  "enabled": true,
  "speed": 1.5
}
```

To write a literal to the graph, use `value` and `type`:

```json
{
  "bEnabled": {
    "value": true,
    "type": 0
  }
}
```

## Where a value can come from

`value` and `else` accept the same forms:

| JSON | Meaning |
| --- | --- |
| `true`, `false`, or a number | A literal value |
| `"baseSpeed"` | The result of an earlier definition in the same file |
| `"0x802~MyMod.esp"` | The current value of that `TESGlobal` |
| `["II_AnimationSpeed", 2]` | The current value of a graph variable |
| `[24]` or `[6, 88]` | The result of a vanilla condition function |

A string in `0x...~Plugin` format must resolve to a `TESGlobal`. Any other
string must name an earlier definition.

Provider arguments are literals only: finite numbers or plugin-qualified FormID
strings. They cannot name helpers or graph variables, and a FormID string passes
the Form itself rather than reading a `TESGlobal` value.

Definitions may refer only to keys written above them in the same file. A
forward reference, self-reference, or unknown name rejects the file. An unused
helper is not calculated, and a helper is recalculated each time it is used.

All calculations use float values. DAF converts only the final graph output:

- Boolean: `0` becomes `false`; any other finite value becomes `true`.
- Integer: the value is range-checked and truncated toward zero.
- Float: the value is written unchanged.

## Conditions and `else`

Put the Creation Kit conditions in a `BGSPerk` record's top-level condition
list, then add that perk's plugin-qualified FormID:

```json
{
  "II_AnimationSpeed": {
    "value": 1.5,
    "conditions": [
      "0x800~MyMod.esp"
    ],
    "post": {
      "multiply": 2
    },
    "else": 1.0,
    "type": 2
  }
}
```

If the perk conditions pass, the result is `3`. If they fail, the result is
`1`. `post` applies only to `value`, not to `else`.

When several perks are listed, the gate passes if any one of them passes. The
conditions inside each perk retain their normal Creation Kit behavior. They are
evaluated with the animation actor as Subject and the event reference, when one
exists, as Target. Without `conditions`, or with an empty array, `value` is
always used.

If `else` is omitted, it defaults to `0`. It may also use an earlier definition,
a `TESGlobal`, a graph read, or a condition-function provider.

To post-process both outcomes, put the conditional choice in a helper:

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

## Post-processing

`post` operations run from top to bottom in the order written:

| Operation | Example |
| --- | --- |
| Add | `"add": 1` |
| Subtract | `"subtract": 1` |
| Multiply | `"multiply": -1` |
| Divide | `"divide": 2` |
| Power | `"pow": 2` |
| Clamp | `"clamp": [0, 1]` |
| Arcsine | `"asin": true` |

An arithmetic operand or either clamp limit may be a number or the name of an
earlier definition:

```json
{
  "source": 0.5,
  "scale": 2,
  "offset": -0.25,

  "fResult": {
    "value": "source",
    "post": {
      "multiply": "scale",
      "add": "offset",
      "clamp": [-1, 1]
    },
    "type": 2
  }
}
```

`asin` accepts inputs from `-1` to `1` and returns radians. Division by zero
produces `0`. An unknown operation rejects the file. Invalid math at runtime
skips the animation using that file and logs the reason.

## Reading graph variables

Use `[name, type]` to read the current value from the animation graph:

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

This reads the current float `II_AnimationSpeed`, divides it by two, and writes
the result back. The read type uses the same `0`/`1`/`2` table as `type` and must
match the graph variable.

A plain string such as `"previousSpeed"` recalculates an earlier definition each
time it is used. It does not read the graph. Use `["previousSpeed", 2]` only when
`previousSpeed` is the name of a real graph variable.

Within one file, use a plain string to build on an earlier definition. A graph
read sees the value already in the graph; DAF does not write any of the file's
outputs until all of them have been calculated.

DAF does not reset values after an animation. A later animation can therefore
read a graph variable that an earlier animation changed, unless the behavior or
another mod changes it first.

## Vanilla condition functions (advanced)

A provider array uses the numeric result of a vanilla condition function. Its
form is `[providerID, argument1, argument2]`:

```json
{
  "scale": {
    "value": [24]
  }
}
```

The provider ID is the function index shown in the
[Creation Kit condition-function list](https://ck.uesp.net/wiki/Condition_Functions).
Vanilla IDs occupy `0` through `735`. Higher IDs are reserved and unavailable
in this build. A call is usable only if DAF supports that function's parameters
and Skyrim exposes it as a condition function.

DAF supplies the animation actor as Subject. Arguments are handled as follows:

- If the function's first parameter is exactly `ObjectRef` and the event has a
  Target, DAF supplies that Target automatically.
- Otherwise, the first config argument supplies the function's first parameter.
- After an automatic Target, the first config argument supplies the function's
  second parameter.
- Form and reference arguments use plugin-qualified FormID strings such as
  `"0x14~Skyrim.esm"`. Numeric and enum arguments use JSON numbers.
- Missing required arguments that DAF cannot supply, extra arguments, or
  incompatible arguments reject the call.

When an eligible Target exists, DAF always uses it; a config argument cannot
replace it. Events whose item is not a reference have no Target.

Examples:

| Function | Source | Result or requirement |
| --- | --- | --- |
| `GetScale()` | `[24]` | Scale of Subject |
| `GetDistance(ObjectRef)` | `[1]` | Distance from Subject to Target; requires Target |
| `GetPos(Axis)` | `[6, 88]` | Subject's X position; use `89` for Y or `90` for Z |
| `GetWithinDistance(ObjectRef, Float)` | `[639, 256]` | Uses Target and a distance of 256; requires Target |

Only functions available as Creation Kit conditions are eligible. Functions
with more than two parameters are unsupported. DAF checks each call while
loading the file; unsupported calls are rejected and the log explains why.
Numeric parameters are currently supported only for `GetPos` (`6`) and
`GetWithinDistance` (`639`); other numeric signatures are rejected. The
`GetWithinDistance` distance must be finite, nonnegative, a whole number, and
within the 32-bit unsigned range.

A provider that cannot produce a value at runtime skips the animation. It does
not use `else`, because `else` is selected only when the definition's
`conditions` do not pass.

## Errors and runtime behavior

- A missing or invalid referenced variable file rejects its animation-data file.
  Other animation-data files still load.
- Before an animation, DAF calculates all graph outputs in its variable file.
  Helpers are calculated only when an output refers to them.
- Calculation or conversion failure makes no graph writes. Graph variables are
  then set in file order; if a later setter fails, earlier changes remain even
  though the animation is skipped.
- A bad graph name or type, missing Target, failed provider, or invalid
  calculation skips that animation and logs the reason. Later animations in the
  queue still run.
- DAF never resets graph variables automatically.

Check the DAF log first when an animation is skipped or an animation-data file
does not load.
