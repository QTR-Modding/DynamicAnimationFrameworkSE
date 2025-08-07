#pragma once
#include <shared_mutex>
#include "Presets/PresetInterface.h"

#define DLLEXPORT __declspec(dllexport)

extern "C" DLLEXPORT DAF_API::AnimEventID ProcessRequestEventID(const char* a_event,int a_major, int a_minor);
extern "C" DLLEXPORT int ProcessSendEvent(DAF_API::AnimEventID a_event, uint32_t a_actor, uint32_t a_form);

namespace Service {
    inline std::shared_mutex m_service_;
    inline DAF_API::AnimEventID custom_event = Presets::AnimEvent::kTotal;
    inline std::unordered_map<std::string, DAF_API::AnimEventID> custom_events;

    DAF_API::AnimEventID AddCustomEvent(const std::string& a_event_name);
}