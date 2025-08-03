#include "logger.h"
#include "Presets/PresetInterface.cpp"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        // Start
		logger::info("Data loaded message received.");
    }
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
		logger::info("Post load message received.");
    }
    if (message->type == SKSE::MessagingInterface::kDeleteGame) {
		logger::info("delete game message received.");
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    Presets::Load();

    return true;
}