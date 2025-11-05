#include "Service.h"
#include "Manager.h"

DAF_API::AnimEventID ProcessRequestEventID(const char* a_event, int a_major, int a_minor) {
    if (a_major != DAF_API::MAJOR || a_minor != DAF_API::MINOR) {
        return 0; // Version mismatch
	}

    std::shared_lock lock(Service::m_service_);
    if (Service::custom_events.contains(a_event)) {
        return Service::custom_events.at(a_event);
    }

	return 0;
}

int ProcessSendEvent(DAF_API::AnimEventID a_event, uint32_t a_actor, uint32_t a_form)
{
    return Manager::GetSingleton()->PlayAnimation( {
        a_event, RE::Actor::LookupByID<RE::Actor>(a_actor),
       RE::TESForm::LookupByID(a_form)
    });

}

DAF_API::AnimEventID Service::AddCustomEvent(const std::string& a_event_name) {
    std::unique_lock lock(m_service_);
    if (custom_events.contains(a_event_name)) {
        return custom_events.at(a_event_name);
	}
    const DAF_API::AnimEventID new_id = ++custom_event;
    custom_events[a_event_name] = new_id;
	return new_id;
}
