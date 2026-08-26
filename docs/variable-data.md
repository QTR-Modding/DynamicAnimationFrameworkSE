# DAF variable data and animation graph variables

> Status: implemented locally; representative Skyrim AE 1.6.1170 runtime
> smoke tests passed, while the full dual-runtime acceptance matrix remains
> pending.
> Items under **Pending validation** are deliberately not claimed as verified.
>
> Last updated: 2026-08-26.

## Purpose

DAF calculates values immediately before an animation graph event starts and
writes selected results to the subject's animation graph. The authoring format
remains small while allowing values to come from:

- literals;
- other calculated variables;
- live animation graph variables;
- `TESGlobal` records;
- the numeric result of supported Skyrim condition functions;
- post-processing of any of the above.

All sources use one `value` field and one recursive evaluator. Authors do not
declare a separate source kind. A top-level definition declares `type` only when
it is an animation graph output; definitions without `type` are helpers.

## Terminology

- **DAF trigger event**: the DAF event that selects an animation-data block, such
  as Activate, ItemPickup, or a custom DAF event.
- **Animation graph event**: an entry in the animation-data block's existing
  `animations` array.
- **Variable group**: one JSON file under `varData` containing named value
  definitions.
- **Subject**: the object reference whose animation is being played and whose
  graph variables are written.
- **Target**: the second object reference associated with the animation request,
  when one exists.

The generic evaluator and provider interfaces accept `TESObjectREFR` for both
positions. In the current queued-animation integration, QTR's `before_play`
callback supplies an `Actor` as Subject. Target is present only when the DAF
event's `a_item` is itself an object reference and its retained handle still
resolves immediately before playback.

## File layout and animation matching

Variable files use a new directory alongside the existing `animData` directory:

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

An animation-data file adds a `variables` array parallel to `animations`:

```json
{
  "animations": ["Take", "TakeLow", "TakeHigh"],
  "variables": ["take", "takeLow", "takeHigh"]
}
```

The arrays match by index:

```text
animations[i] <-> variables[i]
```

When `variables` is present, it must have exactly the same number of entries as
`animations`. Each entry is either a bare variable-group name or `null`. A
`null` entry explicitly means that the animation at the same index has no
variable group:

```json
{
  "animations": ["Take", "TakeLow", "TakeHigh"],
  "variables": [null, "takeLow", "takeHigh"]
}
```

This is a match between each animation graph event and its variable group. It is
not an automatic match based on the animation asset's filename.

DAF derives the namespace from the animation-data file's owning folder and appends
the `.json` extension. For example, `animData/MyMod/interactions.json` resolves
`take` only as:

```text
varData/MyMod/take.json
```

Authors therefore write only the bare variable-group name in `variables`, without
a path, duplicated mod-folder name, or `.json` extension. The same-folder rule
prevents accidental cross-mod lookup.

The implemented name check rejects an empty name, `.` or `..`, `/`, `\\`, `:`,
and any name already ending in `.json` (case-insensitive). Resolution is valid
only for an animation file under `DAF/animData/<folder>` and produces exactly
`DAF/varData/<folder>/<name>.json`. Duplicate top-level `variables` members are
rejected. When present, `variables` must be an array exactly as long as the
parsed `animations` array, and every entry must be a string or `null`.

The `variables` field is omitted entirely when none of the animations need a
variable group, so existing animation data remains valid. When it is present,
authors use `null` placeholders rather than shortening or offsetting the array.
When a group is associated with an animation, DAF evaluates and writes it
immediately before sending that corresponding animation graph event. A chain's
groups are not all evaluated once when the outer DAF trigger is first selected.
This uses ClibUtilsQTR 1.0.7's per-entry `before_play(RE::Actor*, const
Animation&)` callback. Returning `false` follows QTR's existing failed-play path
and suppresses that entry's event or idle dispatch.

## Load-time compilation and validation

JSON is only the authoring and serialization format. DAF reads `animData` and
`varData` once during configuration loading and converts them into owned C++ data.
The runtime animation path never returns to the JSON documents, reparses JSON, or
looks up definitions through RapidJSON values.

The compiled C++ representation contains the variable definitions, typed source
kind, conditions, fallback, post operations, resolved variable dependencies, and
native/provider metadata needed at runtime.

The implemented parser is strict. The variable-group root must be an object;
definition names must be non-empty and unique. Expanded definitions require
`value`, accept only `value`, `else`, `conditions`, `post`, and `type`, and reject
every duplicate or unknown field (including the removed `set`). `conditions`
must be an array of resolvable `BGSPerk` FormIDs. `post` must be an object and
rejects duplicate or unknown operations. Every accepted numeric literal must be
finite and representable as the evaluator float.

During this initial load, DAF processes variable definitions in their written
order. A variable may reference only another variable already defined above it in
the same file. This applies to variable references in `value`, `else`, and
`post`. A forward reference, self-reference, or unknown reference rejects the
entire variable-group config before it can be attached to an animation.

Because every accepted dependency points to an earlier definition, recursion is
impossible. No separate runtime recursion check is needed.

An animation-data file is accepted only when its complete `variables` mapping
and every referenced variable-group file load and validate successfully. A
missing or invalid referenced variable file rejects that entire animation-data
file; DAF does not keep or queue a partial animation sequence from it. Other
animation-data files remain independent and continue loading normally. The log
identifies both the rejected animation-data file and the missing or invalid
variable file.

## Variable-group format

A variable-group file is one JSON object whose keys form a shared namespace of
output and intermediate variables:

```json
{
  "bAlwaysEnabled": {
    "value": true,
    "type": 0
  },

  "bUseLeftHand": {
    "value": ["bUseLeftHand", 0],
    "type": 0,
    "post": {
      "multiply": -1,
      "add": 1
    }
  },

  "fDistance": {
    "value": [1],
    "type": 2,
    "conditions": [
      "0x801~MyMod.esp"
    ],
    "post": {
      "multiply": 0.01,
      "add": 0.0,
      "clamp": [0.0, 1.0]
    },
    "else": 0.0
  },

  "fGlobalValue": {
    "value": "0x802~MyMod.esp",
    "type": 2,
    "post": {
      "clamp": [-1.0, 1.0]
    }
  },

  "fInternalCopy": {
    "value": "fDistance"
  }
}
```

### Compact and expanded definitions

A bare literal is shorthand for an object containing only `value`:

```json
"bEnabled": true
```

is equivalent to:

```json
"bEnabled": {
  "value": true
}
```

Because neither form contains `type`, both examples define a helper. Bare
shorthand is necessarily helper-only. An animation graph output must use object
form and contain both `value` and `type`; for example, a literal true Boolean
output is:

```json
"bEnabled": {
  "value": true,
  "type": 0
}
```

The expanded form can additionally contain:

- `conditions`: condition containers that gate the primary value;
- `else`: the fallback when the gate does not pass; accepts the same source forms
  as `value` and defaults to `0.0f` when omitted;
- `post`: operations applied to the calculated value;
- `type`: the required numeric DAF graph-output type when this definition is an
  animation graph variable. `0` means Boolean, `1` means integer, and `2` means
  float. Omitting `type` makes the definition helper-only.

`type` must be an integer with one of those three values. A non-integer or unknown
value rejects the entire variable-group config at load time. `set` is no longer
part of the schema; any definition containing `set` is rejected at load time,
as is every other unknown or duplicate definition field. There are no separate
`function`, `global`, or `variable` fields.

### Meaning of `value`

The JSON shape determines the source:

| `value` shape | Meaning |
| --- | --- |
| Boolean or number | Direct literal |
| Two-element array beginning with a string | Live animation-graph-variable read: name followed by read type |
| Array beginning with a numeric ID | Condition-function/provider ID followed by signature-dependent author arguments |
| Plugin-qualified FormID string | `TESGlobal` value |
| Any other variable-name string | Recursively calculated variable |

A variable-name string must identify a definition written earlier in the same
file.

The same source rules apply to `else`: it may be a Boolean or number literal, an
earlier variable name, a plugin-qualified `TESGlobal`, a live graph-variable read,
or a function/provider array. DAF evaluates the selected fallback source normally,
then returns its result without applying the current definition's `post`.

Examples:

```json
"value": true
```

```json
"value": 0.25
```

```json
"value": "fDistance"
```

```json
"value": "0x802~MyMod.esp"
```

```json
"value": [1]
```

```json
"value": ["bUseLeftHand", 0]
```

A string-first, two-element array has the shape `[graphVariableName, readType]`
and reads live animation-graph state. `readType` is the numeric DAF enum `0` for
Boolean, `1` for integer, or `2` for float; it selects the matching typed graph
getter. DAF normalizes the getter result to the evaluator's float (`false` to
`0.0f`, `true` to `1.0f`, integer to its numeric float value, and float unchanged).
This source form is valid wherever the normal value-source grammar applies,
including `value` and `else`. The enclosing definition's `type` remains the
destination write type and is independent of the read source's `readType`.

A live read whose graph-variable name matches the enclosing config definition
reads external graph state; it is not a config-variable self-reference or cycle.
If the selected typed getter fails, evaluation of the current variable group
fails and the current animation is skipped through the existing `false` path.
Numeric-first arrays remain function/provider calls.

For a function/provider array, the first element is its numeric ID. Remaining
entries are explicit parameters supplied by the author and interpreted using the
provider's declared signature. Each author argument is either a JSON number or a
JSON string. A number supplies a numeric parameter whose accepted range and
callback-slot encoding come from DAF's verified codec for that signature. The
declared metadata identifies the logical parameter type, but does not by itself
prove its native callback representation. A string always identifies a Form; it
is never passed to the callback as text.

Provider arguments after the ID are literals only. They cannot name another
variable or use any recursively calculated source. JSON numbers supply numeric or
enum parameters, and plugin-qualified FormID strings supply Form or reference
parameters. Dynamic choice between provider calls can still be expressed by a
variable definition's existing `conditions` and `else` branches; it does not make
the arguments inside either provider array dynamic.

Context disambiguates FormID strings. A root `value` FormID resolves as a
`TESGlobal` source. A Form identifier string inside a provider-call array is
resolved through ClibUtilsQTR's `FormReader::GetFormFromString`, checked against
the Form or reference type required by that parameter, and passed as the resolved
runtime pointer. It does not become another `TESGlobal` value source.

Subject and a compatible Target are provided separately according to the
signature rules below.

## One recursive float evaluator

DAF uses one conceptual operation for every named definition:

```text
CalculateVariable(name, context) -> float
```

The evaluation model is:

1. Loop the top-level definitions that contain `type`. Only those declared
   animation graph outputs are evaluation roots.
2. Call `CalculateVariable` for each root that will actually be set.
3. Inside every `CalculateVariable` call, whether for a root or dependency, check
   that definition's `conditions`. When they fail, return its explicit `else` or
   `0.0f` when `else` is omitted, without applying `post`. When they pass,
   calculate `value`.
4. If that calculation references an earlier named variable, call
   `CalculateVariable` for the dependency on demand. Repeat recursively for
   dependencies of dependencies and for named post operands.
5. Apply that definition's `post` operations in their written order.
6. Return the definition's final, post-processed float.
7. Convert and write only a root result, using its declared `type` to select the
   matching animation graph setter.

Calculated results are not cached or memoized. Every reference to a dependency
calls `CalculateVariable` again, including that dependency's conditions, source,
recursive dependencies, and post-processing. Runtime evaluation uses only the
validated C++ definitions; it neither reads JSON nor performs another recursion
check. Definitions that contain no `type` and are not reachable dependencies are
never calculated.

Helpers are not a different kind of calculated value. They omit `type` and use
exactly the same float evaluator as graph outputs, but are evaluated recursively
only when referenced.

## Numeric type model and graph writes

All calculated data stays numeric internally:

- direct Boolean values normalize as `false -> 0.0f` and `true -> 1.0f`;
- literals become floats;
- `TESGlobal` values are floats;
- native condition callbacks write a `double`, which DAF converts to float;
- dependencies and post-processing remain floats.

Every animation graph output declares a numeric DAF `type`: `0` for Boolean, `1`
for integer, or `2` for float. Helpers declare no `type`. All evaluation remains
float; only the final output conversion depends on the declared type, which also
selects `SetGraphVariableBool`, `SetGraphVariableInt`, or
`SetGraphVariableFloat`.

The author-declared numeric `type` is authoritative. DAF does not probe typed
getters, inspect runtime Havok graph metadata to discover the type, or maintain a
cache of discovered graph-variable types.

For a valid calculated float, the final conversion is:

- Havok Boolean: exactly `0.0f` becomes `false`; every other value becomes
  `true`;
- Havok integer: first require that the value is within the representable integer
  range, then truncate toward zero (`3.8f -> 3`, `-3.8f -> -3`); an
  out-of-range value is variable-processing failure;
- Havok float: write the value unchanged.

DAF performs no implicit Boolean thresholding and no integer rounding.

Presence of `type` marks a definition as an animation graph output:

```json
"bEnabled": {
  "value": true,
  "type": 0
}
```

DAF loops that entry, calculates it, converts the finite float result using the
Boolean rule above, and calls `SetGraphVariableBool` for `bEnabled`. A helper
omits `type`:

```json
"fIntermediate": {
  "value": 1.0
}
```

DAF does not loop or write that helper. It calculates it only when another
definition references `fIntermediate` from `value`, `else`, or `post`. Supplying
`type` is therefore the explicit decision to write the same-name animation graph
variable.

## Post-processing

`post` transforms only the primary `value` selected by passing conditions. It
does not transform an `else` fallback. Arithmetic operands may be literal numbers or
variable names directly. A variable operand is another
dependency resolved through the same recursive evaluator; it does not depend on
top-level JSON or graph-write order:

```json
"post": {
  "multiply": -1.0,
  "add": "fOther"
}
```

No wrapper such as `{ "variable": "fOther" }` is required.

The agreed arithmetic vocabulary is:

- `add`;
- `subtract`;
- `multiply`;
- `divide`;
- `pow`.

`divide` must check its divisor before performing the operation. This is required
because a condition's default `else`, or any dependency, may legitimately produce
`0.0f`. A zero divisor produces `0.0f`; division must never propagate infinity or
NaN.

`clamp` and the unary inverse-sine transform (`asin`) remain supported. Other
unary transforms should be added only when there is a concrete use case.

`clamp` must be exactly a two-element array `[minimum, maximum]`; each operand is
either a finite numeric literal or an earlier variable name. Evaluation fails if
the resolved minimum is greater than the maximum. The unary form is exactly
`"asin": true`; `false` and all non-Boolean values are rejected at load time.

There is no `square` operation; squaring is:

```json
"pow": 2
```

There is no special negate operation and no need to create a separate negative
copy of a variable. Negation is:

```json
"multiply": -1
```

Post operations are transforms over the current value rather than a second,
general expression language. DAF applies them from top to bottom in the order
written in the `post` object.

Each post-operation name may appear only once in one definition. A duplicate key
such as two `add` operations makes the variable-group file invalid at load time;
DAF never relies on a JSON parser's first-key or last-key behavior. To apply the
same operation again later, the author stores the first stage in an earlier
helper variable and references that helper from the next definition:

```json
{
  "fStep1": {
    "value": "fBase",
    "post": {
      "add": 1,
      "multiply": 2
    }
  },
  "fFinal": {
    "value": "fStep1",
    "type": 2,
    "post": {
      "add": 3
    }
  }
}
```

If a helper or other post operand fails to calculate, that failure propagates to
the current variable group through the normal evaluator failure path.

DAF requires every calculated source result and every post-operation result to
remain finite. Operations such as `asin` outside its `[-1, 1]` domain or an
invalid `pow` normally produce NaN, while numeric overflow may produce infinity.
DAF detects either with `std::isfinite`, fails the current variable group, and
uses the existing skip-current-animation path. It does not silently replace a
non-finite result with `0.0f` or clamp it. The explicit zero-divisor rule above
remains the one exception: division by zero produces `0.0f` before division is
attempted.

## Conditions used as gates

A `TESCondition` is not itself a form and therefore has no plugin-qualified FormID
that a config can reference. DAF's existing pattern uses a `BGSPerk` form as a
container for a condition list. Entries in a variable definition's `conditions`
array use the same plugin-qualified form syntax and are evaluated with the current
Subject and Target context.

This gate is distinct from using a condition function as a numeric `value` source:

- `conditions` decides whether the primary or `else` branch is selected;
- `value: [id, ...]` directly invokes a supported callback and consumes its
  numeric result.

When the gate fails, DAF evaluates the explicit `else` value. If `else` is absent,
the result is `0.0f`. That fallback is returned directly without `post`. To
post-process both possible branches, an author references the conditional
definition from a later variable and applies `post` there.

Multiple condition-container entries use DAF's existing animation-filter
semantics unchanged. An empty `conditions` array passes. When entries are present,
the gate passes when any referenced `BGSPerk` condition container returns true
for the current Subject and Target; otherwise DAF evaluates `else`.

## Subject and Target lifetime

QTR retains the queued Subject as its actor handle and resolves it on the game
thread before invoking `before_play`. DAF captures a Target handle only when the
event's `a_item` is an actual `TESObjectREFR`. The callback resolves that handle
immediately before evaluation and holds the resulting smart pointer for the
duration of the call. If a Target was expected but its handle is invalid or no
longer resolves, the callback returns `false` and the current animation is
skipped. Non-reference `a_item` forms do not become a Target.

Both values are object references when present:

```text
Subject: TESObjectREFR
Target:  TESObjectREFR or absent
```

The evaluator/provider layer is reusable with any `TESObjectREFR` Subject, but
the current queued animation path necessarily supplies an `Actor` because that
is QTR's callback contract. Target remains `TESObjectREFR` or absent.

Condition callbacks and animation graph access must run on the game thread. If a
request originates elsewhere, evaluation is queued onto the game thread rather
than calling engine functions on the originating thread.

## Numeric condition-function providers

### ID namespaces

- `0..735`: vanilla Skyrim script/condition-function table candidates.
- `736` and above: the reserved DAF/community-provider namespace.

An ID below 736 is not automatically usable. DAF validates that its table entry
has a condition callback and a supported signature before calling it. An ID at or
above 736 is never used to index the vanilla table.

Functions are author-facing numeric IDs. Config authors do not write function-name
strings.

The compile-time seam for community provider additions is implemented, but its
table is currently empty because the separate community repository has not yet
been imported. Therefore every ID at or above 736 currently rejects as
unavailable. Once imported, each community addition supplies the equivalent
parameter metadata needed by the call-array
syntax: count, order, type, optionality, and its callback. This design has no
runtime external-DLL registration or collision system. Repository pull-request
review and maintainer-controlled ID assignment prevent duplicate provider IDs.

### What DAF can see as a function signature

All native condition callbacks share one type-erased C++ ABI:

```cpp
using Condition_t = bool(
    RE::TESObjectREFR* a_thisObj,
    void* a_param1,
    void* a_param2,
    double& a_result);
```

The script command table supplies the logical signature metadata separately. For
each `RE::SCRIPT_FUNCTION`, DAF can read:

```text
functionName
referenceFunction
numParams
params[i].paramName
params[i].paramType
params[i].optional
conditionFunction
```

Therefore DAF normalizes a usable entry as:

```text
FunctionName(
  subject: TESObjectREFR,
  declared parameter 0 if present,
  declared parameter 1 if present
) -> numeric result
```

with the callback's Boolean return captured separately. Its exact convention is
the same on both target runtimes: `true` means that the callback supplied a
valid numeric output, including an ordinary result of `0.0`; `false` means that
evaluation did not supply a usable result. It is not the predicate's truth value.

Subject is the callback's `a_thisObj`. It is implicit and is not included in
`numParams`. There are at most two explicit condition-data slots beyond Subject.

There is no universal native `target` argument. Target insertion is a DAF policy:

1. Subject always supplies the callback's `a_thisObj`.
2. When `numParams == 0`, call the function with Subject and no explicit
   parameters.
3. When parameter 1's declared metadata type is ObjectRef and a runtime Target is
   present, Target supplies parameter 1.
4. Otherwise, the author argument at call-array index 1 supplies parameter 1 as a
   literal.
5. Parameter 2 is never filled automatically from Target. It is always supplied
   by the author as a literal: from call-array index 1 when Target filled
   parameter 1, or from call-array index 2 when the author also supplied parameter
   1. The same literal-only rule applies to every remaining required parameter.
6. A missing or incompatible required parameter fails the provider call.

Only ObjectRef metadata enables automatic Target insertion. Actor and other
reference-shaped metadata types require an author-supplied FormID string even
when the runtime Target would satisfy that type.

This rule is deliberately positional. Parameter 2 can itself be an object
reference, but DAF still requires the author to supply it instead of treating it
as another possible Target slot.

`referenceFunction` is not proof that the function accepts Target, and parameter
names are not reliable enough to override the declared parameter types.

### Representative signatures

These representative signatures illustrate both accepted and deliberately
rejected cases in the current implementation. Zero-parameter and Form-only rows
compile; the two explicitly verified numeric rows compile; a different numeric
signature remains rejected even when its metadata category is known.

| ID | Function metadata | Current status and call shape |
| ---: | --- | --- |
| 1 | `GetDistance(ObjectReferenceID)` | Supported Form-only: `(Subject, Target, nullptr, result)` |
| 6 | `GetPos(Axis)` | Supported verified numeric exception: `(Subject, integerSlot(axis), nullptr, result)` |
| 24 | `GetScale()` | Supported zero-param: `(Subject, nullptr, nullptr, result)` |
| 577 | `IsCloserToAThanB(TESObjectREFR, TESObjectREFR)` | Supported Form-only: `(Subject, Target, authorReference, result)` |
| 584 | `GetRelativeAngle(ObjectReferenceID, Axis)` | Rejected currently: its Axis slot has not been independently allowlisted |
| 639 | `GetWithinDistance(ObjectReferenceID, Float)` | Supported verified numeric exception: `(Subject, Target, integerSlot(distance), result)` |
| 720 | `GetTargetHeight(TESObjectREFR)` | Supported Form-only call shape; result semantics still pending exact game validation |

For example:

```json
"value": [1]
```

needs no author argument because Target fills `GetDistance`'s one reference slot.

```json
"value": [6, 90]
```

contains an author-supplied `Z` Axis because `GetPos` has no Target-compatible
slot. Exact 1.5.97 and 1.6.1170 disassembly establishes the direct Axis values as
`X = 88`, `Y = 89`, and `Z = 90`.

`[584, 90]` would positionally let Target fill the first reference slot and use
the author Axis for the second, but the current compiler rejects it because ID
584's numeric slot codec is not yet verified and allowlisted on both runtimes.

```json
"value": [639, 250]
```

lets Target fill `GetWithinDistance`'s first reference slot and places the
integral distance `250` directly in its second callback slot. Both target
runtimes convert that slot's unsigned integer value numerically to float. They do
not accept a `float*` or IEEE-754 float bits, so a fractional distance is not
representable through this callback ABI.

```text
value = [577, "0x123~MyMod.esp"]
```

lets Target fill parameter 1 of `IsCloserToAThanB`; the FormID string at call-array
index 1 supplies its object-reference parameter 2.

### Lookup and validation

For a vanilla ID already checked to be below 736, the lookup model is:

```cpp
auto* table = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
auto& function = table[id];
```

The vanilla script opcode is expected to be `0x1000 + id`; DAF can validate that
relationship as a guard against indexing the wrong entry.

Before invocation, DAF rejects:

- a null script-command table;
- an out-of-range vanilla ID;
- a null `conditionFunction`;
- more than two explicit parameters;
- a nonzero `numParams` with null parameter metadata;
- missing required parameters;
- an argument that cannot be converted to its declared parameter type;
- any parameter type whose callback representation DAF does not support.

Table membership alone is insufficient because the script-command table also
contains ordinary script commands. Any entry with a null `conditionFunction` is
outside DAF's provider set and is rejected without considering its other
parameters.

### Generic parameter marshalling

Authors supply the semantic values required by the chosen function, but DAF must
still encode each value for the two type-erased `void*` callback slots. A `void*`
slot can carry either a real pointer or pointer-sized value bits; it does not mean
that every argument is a pointer to separate storage.

`SCRIPT_PARAM_TYPE` currently has 81 named values. The implementation
intentionally classifies all 81 as `kInt`, `kFloat`, `kForm`, or `kUnsupported`;
the set of types used by today's vanilla callbacks is test
evidence and prioritization data, not the boundary of the design. This allows a
future DAF/community provider to use any already-supported metadata type without
requiring provider-specific parsing code.

The first-stage classification is deliberately small:

```text
SCRIPT_PARAM_TYPE -> kInt | kFloat | kForm | kUnsupported
```

This classification determines what JSON shape the author must supply. DAF
retains the original `SCRIPT_PARAM_TYPE` for its exact range, semantic, cast, and
callback-slot validation. Multiple metadata values may share one classification
and implementation block; exhaustive behavior does not require 81 duplicated
conversion bodies. Any future enum value unknown to the installed DAF build
fails safely as `kUnsupported`.

For `kForm`, DAF requires a JSON string, resolves it once through ClibUtilsQTR as
a `TESForm*`, and uses the original metadata type to select the appropriate
`As<ExpectedType>()`. A null resolution or null cast rejects the provider call.
DAF forwards the pointer returned by `As<T>()`, not an unchecked original
`TESForm*`. For example, Quest uses `As<RE::TESQuest>()`, Faction uses
`As<RE::TESFaction>()`, and ObjectRef uses `As<RE::TESObjectREFR>()`; the latter
also accepts Actors. Composite metadata types may try each explicitly permitted
`As<T>()` target or another verified semantic predicate. A Form-shaped metadata
type with no verified mapping remains `kUnsupported`.

Resolved Form and reference arguments use their actual runtime pointers. Numeric
arguments remain typed C++ values after config loading until DAF encodes them for
the callback.

The current invocation allowlist is deliberately narrower than the 81-entry
semantic classifier. It accepts zero-parameter callbacks, callbacks whose
explicit parameters are all supported Form/reference types, and exactly two
verified numeric codecs: ID 6 `GetPos` parameter 0 (`Axis`, direct integer slot)
and ID 639 `GetWithinDistance` parameter 1 (`Float` metadata, finite
non-negative integral `uint32_t`-like direct slot). Other callbacks containing a
numeric parameter are rejected at compilation until their slot representation is
proved on both Skyrim SE 1.5.97 and Skyrim AE 1.6.1170.

Exact binary audits of Skyrim SE 1.5.97 and Skyrim AE 1.6.1170 agree on the
numeric callbacks examined:

- `GetPos` reads the low 32 bits of `a_param1` directly as `88`, `89`, or `90`
  for X, Y, or Z and never dereferences the slot;
- `GetWithinDistance` converts the unsigned integer value held directly in
  `a_param2` to float and never dereferences or IEEE-bit-reinterprets the slot.

For these verified direct-integer codecs, DAF range-checks the author's number,
converts it to `std::uintptr_t`, then places that value directly in the `void*`
slot:

```cpp
auto* slot = reinterpret_cast<void*>(
    static_cast<std::uintptr_t>(value));
```

DAF must not pass `&value`: that would put the local variable's memory address in
the slot. It must not IEEE-bit-pack a float for ID 639: the callback would treat
those bits as a large integer. ID 639 therefore accepts only finite, non-negative,
integral config values.

Metadata identifies the logical argument category, but DAF still validates the
slot codec before supporting a numeric parameter type. It shares one codec across
matching verified signatures and needs a function-specific exception only if the
target executables prove different behavior. This complexity is entirely
internal; authors continue supplying plain JSON numbers.

Across IDs `0..735`, both target runtimes have 397 eligible non-null condition
callbacks. Among those providers, exactly one declares a `Float` parameter:
ID 639 `GetWithinDistance`. Ordinary script-only commands are deliberately
irrelevant to this feature.

Consequently, Dynamic Armor Variants' union-plus-bit-cast float packing cannot be
copied as DAF's generic rule. For ID 639 it would place IEEE-754 bits in the slot,
then `GetWithinDistance` would interpret those bits as an integer distance.

The supported author-argument categories are:

- JSON numbers: the verified callback codec decides whether the value must be
  integral and how it is placed in the slot;
- JSON strings: always Form identifiers, resolved through ClibUtilsQTR's
  `FormReader::GetFormFromString` and checked against the declared Form/reference
  subtype;
- unused optional slots: null after validation.

Native text/string parameters are unsupported because an author-supplied JSON
string is reserved for Form resolution. Script variables, aliases, package data,
event data, and other context-dependent types remain unsupported unless their
known `SCRIPT_PARAM_TYPE` receives an explicit, verified numeric or Form codec.

Authors are responsible for knowing the native meaning of extra parameters. DAF
does not need friendly conversions for every engine enum. For example, Axis stays
numeric in config rather than becoming an author-facing `"Z"` string; authors use
the verified direct values `88`, `89`, and `90`.

### Callback result

The desired numeric value is written through `double& a_result`. It is not the
callback's Boolean return. DAF initializes `a_result` to `0.0` before every call
and converts the resulting `double` into the shared float evaluation pipeline.

Initializing the result prevents an unwritten output from exposing stale data.
Exact `TESConditionItem::IsTrue` disassembly on both supported runtimes establishes
the Boolean convention:

- when the callback returns `false`, the engine skips the numeric comparison and
  forces the whole condition false;
- when it returns `true`, the engine reads `a_result` and performs the selected
  comparison against the CTDA value;
- callbacks representing an ordinary false predicate write `0.0` and still
  return `true`.

Therefore DAF treats a `false` callback return as source-evaluation failure, not
as the numeric value false. It aborts evaluation of the current variable group;
it does not select the definition's `else`. The `else` source is only the fallback
for a normal `conditions` gate failure.

### Variable failure and the existing animation queue

Variable processing reuses the animation queue's existing Boolean failure path.
ClibUtilsQTR's `Animator::UpdateLoop` removes the current animation entry from the
queue before attempting to play it. The existing animation call returns `false`
when `NotifyAnimationGraph` or `PlayIdle` fails. On that result, `UpdateLoop`
restarts the ticker with a 10 ms interval, so the already-popped entry is not
retried and the next queued animation is processed.

DAF therefore extends the current per-animation attempt rather than adding a
second chain-control mechanism:

1. If the animation has no variable group, use the existing animation call.
2. Otherwise, calculate and convert every graph-variable result for that group
   before invoking any setter.
3. If calculation or conversion fails, no setter is called. Prepared outputs are
   then written sequentially. If a later setter returns `false`, earlier setters
   from the same group may already have succeeded; DAF does not attempt rollback.
   Any such failure suppresses the current animation event and returns `false`
   through the same path as an animation-call failure.
4. If variable processing succeeds, write the prepared graph values and perform
   the existing animation call, returning that call's Boolean result.

Consequently, a variable failure skips only its corresponding animation entry.
It does not stop or clear the remaining animation chain. No separate `continue`
or chain-level failure handling is needed.

Runtime failure reporting is per attempt. The evaluator retains the first
concrete failing definition and reason, recursive callers propagate it without
overwriting it, and the public `noexcept` boundary emits one error containing the
compiled group file path, definition name (or `<group>`), and reason. Provider
errors preserve provider ID/name and their specific failure. Exceptions are
caught at that same boundary. There is no process-wide suppression cache, so a
later failed animation attempt produces its own single diagnostic.

### Higher-level CK context is not implied

Direct callback invocation does not reproduce every feature of
`TESCondition::IsTrue`. In particular, a Creation Kit condition's Run On setting
can choose an action reference, target reference, linked reference, quest alias,
package data, event data, or other context before the callback is reached.

The agreed DAF rule is narrower:

- Subject is always `a_thisObj`;
- Target may fill a compatible declared parameter;
- no hidden CK Run On context is fabricated.

A zero-parameter function configured in the CK as “Run On Target” is therefore not
equivalent to this direct-call model. Supporting that later would require an
explicit feature rather than silently changing Subject.

## Pending validation

The schema compiler, 81-entry parameter classifier, provider allowlist,
evaluator, diagnostics, preset mapping, and QTR 1.0.7 `before_play` integration
are implemented locally. The opt-in `DAF_BUILD_TESTS` target builds the real
compiler/provider sources and registers the `VariablesCompilerSchema` CTest.
The recorded local run passes 1/1 tests: nine JSON compiler cases plus
group-name/path validation. Those cases exercise compiler grammar, not native
provider callbacks or the runtime evaluator.

A Skyrim AE 1.6.1170 live smoke test passed the queued `before_play` path,
Boolean graph reads/writes, helper references, post operations, and
skip-current-entry failure behavior. A second AE smoke test passed
zero-parameter providers ID 24 `GetScale` and ID 46 `GetDead`, Form-argument
provider ID 1 `GetDistance`, and numeric Axis provider ID 6 `GetPos`. The
observed results also confirmed that a successful callback may produce numeric
`0.0`.

The remaining acceptance work is:

1. Complete the Skyrim SE 1.5.97 runtime matrix and the remaining AE cases:
   load-time rejection, integer/float graph writes and conversions,
   conditions/fallback, `TESGlobal`, target expiry, sequential setter failure,
   and explicit continuation after a skipped entry.
2. Runtime-test ID 639 `GetWithinDistance` on both target runtimes, repeat the
   representative provider smoke on Skyrim SE 1.5.97, and confirm that a
   callback return of `false` fails the group and produces one diagnostic.
3. Add any further numeric callback codec only after its exact slot semantics are
   verified on both supported runtimes.
4. Import the separate community-provider repository before claiming any ID at
   or above 736 is available.
5. Verify the exact sign and coordinate source returned by `GetTargetHeight`
   before relying on its semantic result.

## Implementation invariants

These constraints summarize the implemented behavior and remaining acceptance
requirements:

- Keep each `animations[i]` entry associated with its `variables[i]` group through
  the animation queue.
- When `variables` is present, require its length to equal `animations`; preserve
  `null` as the explicit no-group entry for that animation index.
- Reject an entire animation-data file at load time when its variable mapping or
  any referenced variable-group file is missing or invalid; never retain a
  partial animation sequence from that file, and do not reject unrelated files.
- Parse JSON once into owned C++ configuration structures and never access the JSON
  documents from animation-time evaluation.
- Compile definitions in written order and resolve every variable reference only
  to a definition already compiled above it; reject the entire group on a forward,
  self, or unknown reference.
- Let QTR retain the queued Actor Subject handle. Retain a Target handle only
  when `a_item` is an actual reference, and fail the current entry if an expected
  Target no longer resolves at `before_play` time.
- Evaluate on the game thread immediately before the graph event is sent.
- Calculate and convert all results before the first setter, then write outputs
  sequentially. Do not claim rollback: if a later setter fails, earlier writes
  may remain.
- Return `false` through the existing per-animation call when variable processing
  fails; rely on the existing popped-entry queue behavior to advance to the next
  animation, without clearing or aborting the remaining chain.
- Build evaluation roots only from definitions containing a valid `type`; evaluate
  definitions without `type` recursively only when referenced, and never eagerly
  calculate every definition in a variable file.
- Use one recursive float evaluator for roots and on-demand dependencies, without
  result caching or memoization.
- Interpret string-first two-element source arrays as live graph-variable reads,
  use their `readType` to select the Boolean, integer, or float getter, and
  normalize the result to the evaluator float. A failed getter fails the current
  variable group; the enclosing definition's `type` still selects the destination
  setter.
- Reject duplicate post-operation keys at load time; require an earlier helper
  variable when the same operation must be applied again.
- Require source and post-operation results to pass `std::isfinite`; a NaN or
  infinity fails the current variable group.
- Reject unsupported native calls before invoking them.
- Accept only literal JSON numbers and FormID strings as provider arguments after
  the ID; never resolve those argument positions as calculated variables or other
  recursive sources. Insert Target automatically only for parameter 1 declared as
  ObjectRef.
- Keep all 81 known `SCRIPT_PARAM_TYPE` values explicitly classified; never infer
  the supported set solely from the current vanilla callback table, and reject
  unknown future values safely.
- Resolve every author-supplied Form string once, validate it through the
  metadata-selected `As<T>()` or verified composite rule, and forward only the
  validated pointer.
- Invoke numeric callbacks only through a slot codec verified on both target
  runtimes, and initialize the callback result to `0.0` before every invocation.
- Reject non-integer or unknown graph-output `type` values and reject the removed
  `set` field at load time. At the final write, use the declared `type` to apply
  the locked conversion and call the matching Boolean, integer, or float setter.
- Log enough context to identify the mod folder, variable file, variable name,
  function ID, and rejection reason without crashing animation playback.

## Evidence references

The binary evidence above comes only from the two relevant target executables:

- Skyrim SE 1.5.97.0, SHA-256
  `693E5A51EA2680119A68620BCF5080E81745549872B5D06BBD3F51131B67ABAB`:
  ID 6 `GetPos` callback `0x1402D4690` and ID 639
  `GetWithinDistance` callback `0x1402D45E0`;
- Skyrim AE 1.6.1170.0, SHA-256
  `C434208894F07F604B852F29B8EDC3A58C4DE63DE783373733E72B2B73F33BE9`:
  ID 6 `GetPos` callback `0x140329D70` and ID 639
  `GetWithinDistance` callback `0x140329CC0`.

The callback Boolean convention is established by the generic condition
evaluator in those same executables:

- Skyrim SE 1.5.97.0: `TESConditionItem::IsTrue` at `0x1404454C0`; after the
  indirect callback call it tests `AL`, forces false when zero, and only otherwise
  compares the callback's `double` output;
- Skyrim AE 1.6.1170.0: `TESConditionItem::IsTrue` at `0x1404A05E0`; it follows
  the same return gate and comparison order.

Representative callbacks such as `GetQuestRunning` return `true` while leaving
their initialized output at `0.0` for an ordinary false predicate, confirming
that the callback Boolean is output validity rather than predicate truth.

For both executables, all 736 table rows satisfy `opcode == 0x1000 + ID` and 397
rows have a non-null condition callback. The direct disassembly behavior is
recorded in **Generic parameter marshalling** above.

DAF's configured CommonLibVR-MIT revision is
`dfb6263ea688a06c9bcf90f36ac46ddeacbf2b1f`. At that revision:

- [`SCRIPT_PARAMETER`](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/dfb6263ea688a06c9bcf90f36ac46ddeacbf2b1f/include/RE/C/CommandTable.h#L131-L141)
- [`SCRIPT_FUNCTION` and `Condition_t`](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/dfb6263ea688a06c9bcf90f36ac46ddeacbf2b1f/include/RE/C/CommandTable.h#L293-L325)
- [script-command lookup](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/dfb6263ea688a06c9bcf90f36ac46ddeacbf2b1f/src/RE/C/CommandTable.cpp#L57-L73)
- [`TESCondition::FUNCTION_DATA` and its two parameter slots](https://github.com/QTR-Modding/CommonLibVR-MIT/blob/dfb6263ea688a06c9bcf90f36ac46ddeacbf2b1f/include/RE/T/TESCondition.h#L887-L895)
- [SKSE vanilla command IDs and declared parameters](https://skse.silverlock.org/vanilla_commands.html)

Existing open-source parsers provide useful comparison evidence, but their
packing must not be assumed correct for DAF's direct callback calls. In
particular, Dynamic Armor Variants' float packing conflicts with the verified ID
639 ABI on both target runtimes. Exact DAF runtime probes remain the acceptance
test:

- [Dynamic Armor Variants condition parser](https://github.com/Exit-9B/DynamicArmorVariants/blob/9c805c3b935254a684862f10d5596fc1640409e1/src/main/ConditionParser.cpp)
- [Papyrus Extender condition parser](https://github.com/powerof3/PapyrusExtenderSSE/blob/160d6272e626d8a120bf9b42d113a631b3bb222a/src/Papyrus/Util/ConditionParser.cpp)
