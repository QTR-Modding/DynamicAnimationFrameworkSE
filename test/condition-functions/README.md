# Condition-function sweep

Build DAF in Debug, install this directory as an MO2 mod, and pick up one loose
Gold001 reference. The five-second delay keeps Target alive during evaluation.

The DAF log contains one trace line per calculated function. The final graph
variable setter failure is expected: the typed definition names exist only to
make DAF calculate every root and are not real animation graph variables.

The fixture covers every Skyrim condition function from IDs 0-735 whose
parameters DAF supports, plus CommunityFunctionsSE ID 1000. These Skyrim
functions require parameter payloads that DAF intentionally does not support:

- 53 `GetScriptVariable`
- 79 `GetQuestVariable`
- 98 `GetPlayerControlsDisabled`
- 325 `GetWithinPackageLocation`
- 407 `GetVATSValue`
- 447 `GetGraphVariableFloat`
- 576 `GetEventData`
- 611 `IsNullPackageData`
- 629 `GetVMQuestVariable`
- 630 `GetVMScriptVariable`
- 675 `GetGraphVariableInt`

ID 726, `DoesNotExist`, is an unused command-table sentinel rather than a callable condition function.
