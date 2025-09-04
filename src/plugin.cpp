#include "Hooks.h"
#include "logger.h"
#include "Presets/PresetInterface.h"

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void OnMessage(SKSE::MessagingInterface::Message* message) {  // NOLINT(misc-use-internal-linkage)
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::Install();
        Presets::Load();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}