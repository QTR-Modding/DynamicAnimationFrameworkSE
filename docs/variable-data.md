# DAF variable data

DAF calculates and sets animation graph variables immediately before each
animation is played. All calculated sources use the same small value language.

JSON is read only while presets load. DAF validates it and stores owned C++
data; animation-time evaluation never reparses or refers back to JSON.

## Animation-to-variable mapping

Variable files live beside the existing `animData` tree:

```text
Data/SKSE/Plugins/DAF/
  animData/
    MyMod/
      interactions.json
  varData/
    MyMod/
      take.json
      takeLow.json
      takeHigh.json
```

An animation-data file may add a `variables` array parallel to `animations`:

```json
{
  "animations": ["Take", "TakeLow", "TakeHigh"],
  "variables": ["take", null, "takeHigh"]
}
```

The relationship is strictly by index:

```text
animations[i] <-> variables[i]
```

Rules:

- Omitting `variables` means that none of the animations uses variable data.
- When present, `variables` must be an array with exactly as many entries as
  `animations`.
- Each entry is a bare group name or `null`. `null` means no group for that
  animation.
- A name contains no path and no `.json` extension. `take` in
  `animData/MyMod/interactions.json` resolves only to
  `varData/MyMod/take.json`.
- Empty names, `.`, `..`, path separators, `:`, and names ending in `.json` are
  rejected.
- A missing or invalid referenced group rejects the whole animation-data file;
  unrelated animation-data files still load normally.

Omitted or `null` variable data performs no graph writes or resets. Existing
graph state remains unchanged.

This maps an animation graph event to a variable group. It does not infer a
group from an animation asset filename.

## Variable-group format

A group is one JSON object. Definition names must be nonempty and unique. Each
key names either a helper or the graph variable that DAF will set:

```json
{
  "fBase": 100.0,

  "fNegated": {
    "value": "fBase",
    "post": {
      "multiply": -1.0
    }
  },

  "bAlwaysEnabled": {
    "value": true,
    "type": 0
  },

  "bUseLeftHand": {
    "value": ["bUseLeftHand", 0],
    "post": {
      "multiply": -1.0,
      "add": 1.0
    },
    "type": 0
  },

  "fDistance": {
    "value": [1],
    "conditions": [
      "0x800~MyMod.esp"
    ],
    "else": 0.0,
    "post": {
      "multiply": 0.01,
      "clamp": [0.0, 1.0]
    },
    "type": 2
  },

  "fGlobalValue": {
    "value": "0x802~MyMod.esp",
    "post": {
      "clamp": [-1.0, 1.0]
    },
    "type": 2
  },

  "iResult": {
    "value": "fNegated",
    "type": 1
  }
}
```

An expanded definition accepts these fields:

| Field | Meaning |
| --- | --- |
| `value` | Required source to calculate. |
| `conditions` | Optional `BGSPerk` condition containers gating `value`. |
| `else` | Optional fallback source; defaults to `0.0`. |
| `post` | Optional ordered transformations of the primary value. |
| `type` | Marks a graph output and chooses its setter type. |

Unknown or duplicate fields reject the group. `set` is invalid; `type` both
marks a graph output and declares its type. There are no separate `function`,
`global`, or `variable` fields.

A bare Boolean or number is shorthand for an object containing only `value`:

```json
"fBase": 1.5
```

Because shorthand has no `type`, it always defines a helper. A literal graph
output uses expanded form:

```json
"bEnabled": {
  "value": true,
  "type": 0
}
```

### Value sources

The JSON shape identifies the source:

| `value` or `else` | Source |
| --- | --- |
| Boolean or number | Literal |
| Earlier variable-name string | Recursively calculated definition |
| Plugin-qualified FormID string | `TESGlobal` value |
| `[graphName, readType]` | Live animation graph variable |
| `[providerID, args...]` | Numeric condition-function provider |

`else` accepts every source shape accepted by `value`.

A string-first array must contain exactly a nonempty graph variable name and a
read type. A numeric-first array is a provider call. Arguments inside provider
arrays are literals only; they are not variable references.

## Evaluation model

DAF uses one recursive float evaluator for every definition:

1. Only definitions containing `type` are root outputs.
2. For each root, evaluate its conditions.
3. If the gate passes, calculate `value` and apply `post` in written order.
4. If the gate fails, calculate explicit `else`, or use `0.0` when omitted.
   The fallback bypasses that definition's `post`.
5. A reference to another definition calculates it recursively on demand.
6. Convert and write only root results.

Definitions without `type` are helpers. DAF never evaluates an unused helper
just because it appears in the file. Results are not cached: each reference
reevaluates that dependency, including its conditions and post-processing.

Definitions are compiled in written order. `value`, `else`, and post operands
may reference only definitions already written above them in the same file.
Forward, self, and unknown references reject the group at load time. This rule
also makes cycles impossible, so runtime cycle detection is unnecessary.

All literals and intermediate results must be finite and representable as a
float. Boolean literals normalize to `0.0` or `1.0`; globals, provider results,
dependencies, and post-processing also enter the same float pipeline.

## Graph types and graph reads

`type` and a live graph read's `readType` use the same numeric enum:

| Value | Graph type |
| ---: | --- |
| `0` | Boolean |
| `1` | Integer |
| `2` | Float |

The author-declared type is authoritative. DAF does not inspect Havok graph
metadata or probe getters to discover it.

A root's key is the destination graph variable name. Final conversions are:

- Boolean: exactly `0.0` is false; every other finite value is true.
- Integer: require the representable integer range, then truncate toward zero.
- Float: write unchanged.

DAF then calls the matching typed graph setter. A failed conversion or setter
fails that variable group.

A live read such as `"value": ["bUseLeftHand", 0]` calls the matching typed
getter and normalizes its result to float. Its `readType` is independent of the
enclosing output's `type`. A failed getter fails the group.

A later definition may reference an earlier graph-output definition by its key;
that recursively recalculates the definition. To read the actual current graph
state, use `[graphName, readType]`. Graph writes remain in the animation graph
after an animation finishes, so a later animation's group can read a value set
by an earlier one.

## Conditions and fallback

A `TESCondition` is not a Form, so configs cannot identify one directly. Each
entry in `conditions` is a plugin-qualified FormID resolving to a `BGSPerk` whose
condition list is evaluated using the current Subject and Target context.

The existing DAF filter semantics apply: an empty array passes; otherwise the
gate passes when any listed condition container passes. A normal gate failure
selects `else` or its default `0.0`.

This is separate from a provider call. A provider callback returning false is an
evaluation failure, not a condition-gate failure, and therefore does not select
`else`.

To post-process both branches, put the conditional result in an earlier helper
and apply `post` from a later definition.

## Post-processing

`post` transforms the current primary value from top to bottom:

| Operation | Operand |
| --- | --- |
| `add` | Number or earlier variable |
| `subtract` | Number or earlier variable |
| `multiply` | Number or earlier variable |
| `divide` | Number or earlier variable |
| `pow` | Number or earlier variable |
| `clamp` | `[minimum, maximum]`; each is a number or earlier variable |
| `asin` | Exactly `true` |

Examples:

```json
"post": {
  "multiply": -1,
  "add": "fOffset",
  "clamp": [-1, 1]
}
```

Negation is multiplication by `-1`; squaring is `pow: 2`. There are no separate
`negate` or `square` operations.

Each operation name may appear only once in a definition. If an author needs the
same operation in two stages, the first stage goes in an earlier helper.

Special failure rules:

- Division by positive or negative zero produces `0.0` without dividing.
- `clamp` fails when its evaluated minimum exceeds its maximum.
- NaN or infinity from `pow`, `asin`, overflow, or any other calculation fails
  the group rather than being silently replaced or clamped.

## Numeric condition-function providers

Provider syntax is:

```json
"value": [providerID, arg1, arg2]
```

IDs `0..735` refer to vanilla Skyrim command-table candidates. IDs `736` and
above are reserved for future community additions and are unavailable in this
build. Authors always use numeric IDs, not function-name strings.

The native callback ABI is:

```cpp
bool Condition(
    RE::TESObjectREFR* subject,
    void* parameter1,
    void* parameter2,
    double& result);
```

Skyrim's metadata separately provides the function name, parameter count,
parameter types, optional flags, and callback. Subject is implicit and is not
included in `numParams`. The callback exposes two parameter slots, so DAF rejects
metadata declaring more than two parameters.

DAF binds them as follows:

1. Subject always becomes the callback's first argument.
2. With zero declared parameters, no config argument is needed.
3. If declared parameter 0 is exactly `ObjectRef` and DAF has a Target, Target
   fills it automatically.
4. Otherwise call-array `arg1` supplies declared parameter 0.
5. Declared parameter 1 is never filled automatically. `arg1` supplies it when
   Target filled parameter 0; otherwise `arg2` supplies it.
6. Omitted trailing optional parameters become null. Missing required, extra,
   or incompatible arguments fail the call.

Only exact `ObjectRef` metadata enables automatic Target insertion. Actor and
other Form/reference parameters remain author-supplied. Parameter names and the
metadata's `referenceFunction` flag do not override this rule.

Author arguments after the ID are limited to:

- JSON strings for Forms and references. DAF resolves the string with
  ClibUtilsQTR, validates it with the metadata-selected `As<T>()`, and forwards
  the validated pointer. Dynamic Forms that cannot be named are unsupported.
- JSON numbers for a statically verified numeric callback-slot codec.

Strings in provider arrays always mean Forms; they never mean text, globals, or
variable names. Authors supply engine enum values numerically.

DAF explicitly classifies every known `SCRIPT_PARAM_TYPE` as integer, float,
Form, or unsupported, while retaining the exact metadata type for validation.
Unknown future types fail safely. Form/reference types share verified pointer
marshalling. Numeric metadata alone is insufficient because a `void*` slot does
not reveal whether a callback expects a pointer, integer value, or encoded bits.

Zero-parameter callbacks and callbacks whose parameters are supported
Form/reference types need no function-specific marshaller. For example, `[24]`
calls `GetScale()`, `[1]` lets Target supply `GetDistance(ObjectRef)`, and
`[577, "0x803~MyMod.esp"]` lets Target supply the first reference to
`IsCloserToAThanB` while the FormID supplies its second reference.

Only these numeric slot codecs are currently verified and allowlisted on both
Skyrim SE 1.5.97 and Skyrim AE 1.6.1170:

| ID | Function | Supported call |
| ---: | --- | --- |
| `6` | `GetPos(Axis)` | `[6, 88]`, `[6, 89]`, or `[6, 90]` for X, Y, or Z. |
| `639` | `GetWithinDistance(ObjectRef, Float)` | `[639, distance]`; Target fills parameter 0. Distance must be finite, nonnegative, integral, and within `uint32`. |

Other numeric signatures, including `[584, 90]` for `GetRelativeAngle`, are
rejected before invocation until their exact slot representation is verified on
both supported runtimes.

The callback writes the numeric value through `double& result`. Its Boolean
return reports whether a usable result was produced: `true` may still accompany
the valid numeric result `0.0`; `false` fails the group. DAF initializes the
double to zero, invokes the callback, verifies a finite float-range result, and
then enters the common float evaluator.

DAF validates the vanilla opcode relationship, callback presence, parameter
metadata, count, optionality, argument count and types, Form casts, and slot
codec before invoking a provider. Ordinary script commands with no condition
callback are rejected.

Direct provider calls do not reproduce Creation Kit `Run On` contexts such as
quest aliases, package data, linked references, or event data. DAF supplies only
its explicit Subject, optional Target insertion, and author literals.

## Runtime integration and failure behavior

The generic evaluator accepts `TESObjectREFR` Subject and Target pointers. In
the current queued-animation integration, ClibUtilsQTR's `before_play` callback
provides an Actor Subject on the game thread. DAF retains Target only as an
`ObjectRefHandle` when the animation request's item is actually a reference, and
resolves it immediately before evaluation. An invalid or expired expected
Target fails that animation attempt; a non-reference item is simply no Target.

For each animation entry:

1. If it has no variable group, use the existing animation path unchanged.
2. Otherwise calculate and convert every root output before calling any setter.
3. If preparation succeeds, write the outputs sequentially.
4. Then send the existing graph event or idle through QTR.

Calculation or conversion failure causes no writes. Setter writes are not
transactional: if a later setter fails, earlier writes from that group may
remain. Any variable-processing failure returns false through QTR's existing
failed-animation path. The current animation entry is skipped and the already
queued chain continues with its next entry; the failed entry is not retried.

Evaluator exceptions are caught at the public boundary. A failed
variable-processing attempt emits one diagnostic with the group path, failing
definition when available, and the specific reason. There is no global
suppression cache, so a later failed attempt gets its own diagnostic.

Community-provider imports and additional numeric callback codecs are future
work outside this feature.
