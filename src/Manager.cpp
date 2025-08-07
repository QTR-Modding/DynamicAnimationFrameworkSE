#include "Manager.h"
#include "Hooks.h"
#include "Utils.h"

bool Manager::PlayAnimation(RE::Actor* a_actor, const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain)
{
    if (RE::ActorHandlePtr actor; 
        RE::BSPointerHandleManagerInterface<RE::Actor>::GetSmartPointer(a_actor->GetHandle(),actor)) {
        Animator* animator = nullptr;
        if (std::shared_lock lock(m_animators_); animators.contains(actor)) {
            animator = animators.at(actor);
        }
        if (!animator) {
            animator = new Animator(actor);
            std::unique_lock lock(m_animators_);
            animators[actor] = animator;
        }

        return animator->Add2Q(anim_chain);
        /*if (!animator->isRunning()) {
            return true;
        }*/
    }

    return false;
}

int Manager::PlayAnimation(DAF_API::AnimEventID a_animevent, RE::TESObjectREFR* a_actor, RE::TESForm* a_form)
{
    if (const auto actor = a_actor->As<RE::Actor>()) {
        if (!ActorCheck(actor)) {
            return 0;
        }

        if (const auto anim_data = GetAnimData(a_animevent,{.actor_id= a_actor->GetFormID(),.form= a_form}); 
            !anim_data.animations.empty()) {
            
            if (PlayAnimation(actor,{a_animevent,anim_data.animations})) {
                if (auto attach_node = anim_data.attach_node; !attach_node.empty()) {
                    if (const auto actor_id = a_actor->GetFormID(); !Hooks::item_meshes.contains(actor_id)) {
                        RE::NiPointer<RE::NiAVObject> a_model;
                        if (Utils::GetModel(a_form, a_model); a_model) {
						    Hooks::item_meshes[actor_id] = {a_model, attach_node};
                        }
                    }

				}
                return anim_data.delay;
            }
        }
    }
    return 0;
}

bool Manager::ActorCheck(const RE::Actor* a_actor) {
    if (!a_actor->Is3DLoaded()) {
        return false;
    }
    return true;
}

size_t Manager::ActorHandleHash::operator()(const RE::ActorHandlePtr& actor_ptr) const noexcept {
    return std::hash<RE::FormID>()(actor_ptr ? actor_ptr->GetFormID() : 0);
}

bool Manager::ActorHandleEqual::operator(
)(const RE::ActorHandlePtr& lhs, const RE::ActorHandlePtr& rhs) const noexcept {
    if (!lhs || !rhs) {
        return lhs == rhs;
    }
    return lhs->GetFormID() == rhs->GetFormID();
}

Presets::AnimData Manager::GetAnimData(DAF_API::AnimEventID a_animevent, const Filter& filter)
{
    std::map<int,Presets::AnimData*> result;

    const auto filter_actorid = filter.actor_id;
    const auto filter_actor = RE::TESForm::LookupByID<RE::Actor>(filter.actor_id);
    const auto filter_actor_kw = filter_actor ? filter_actor->As<RE::BGSKeywordForm>() : nullptr;
    const auto filter_form = filter.form;
    const auto filter_form_kw = filter_form ? filter_form->As<RE::BGSKeywordForm>() : nullptr;
    const auto filter_formtype = filter_form ? filter_form->GetFormType() : RE::FormType::None;

    if (std::shared_lock lock(Presets::m_anim_data_); Presets::anim_map.contains(a_animevent)) {
        for (auto& anim_data : Presets::anim_map.at(a_animevent)) {
            if (!anim_data.form_types.empty() && !anim_data.form_types.contains(filter_formtype)) {
                continue;
            }
            if (!anim_data.actors.empty() && !anim_data.actors.contains(filter_actorid)) {
                continue;
            }
            if (!anim_data.forms.empty() && !anim_data.forms.contains(filter_form)) {
                continue;
            }
            if (!anim_data.keywords.empty()) {
                std::vector<RE::BGSKeyword*> kws;
                if (filter_form_kw) {
                    filter_form_kw->ForEachKeyword([&kws](RE::BGSKeyword* a_kw){kws.push_back(a_kw);return RE::BSContainer::ForEachResult::kContinue;});
                }
                if (!std::ranges::any_of(kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.keywords.contains(a_kw);})) {
                    continue;
                }
            }
            if (!anim_data.actor_keywords.empty()) {
                std::vector<RE::BGSKeyword*> kws;
                if (filter_actor_kw) {
                    filter_actor_kw->ForEachKeyword([&kws](RE::BGSKeyword* a_kw){kws.push_back(a_kw);return RE::BSContainer::ForEachResult::kContinue;});
                }
                if (!std::ranges::any_of(kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.actor_keywords.contains(a_kw);})) {
                    continue;
                }
            }

            if (!anim_data.locations.empty()) {
                RE::BGSLocation* a_loc = nullptr;
                if (filter_actor) {
                    a_loc = filter_actor->GetCurrentLocation();
                }
                if (!anim_data.locations.contains(a_loc)) {
                    continue;
                }
            }



            result[anim_data.priority] = &anim_data;
        }
        
    }

    if (!result.empty()) {
        return *result.begin()->second;
    }

    return {};
}

void Manager::PauseAnimators()
{
    std::unique_lock lock(m_animators_);
    for (const auto& animator : animators | std::views::values) {
        animator->Pause();
    }
}

void Manager::ResumeAnimators()
{
    std::unique_lock lock(m_animators_);
    for (const auto& animator : animators | std::views::values) {
        animator->Resume();
    }
}

int Manager::OnActivate(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item) {
    return PlayAnimation(Presets::AnimEvent::kActivate,a_actor,a_item);
}

int Manager::OnPickup(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item) {
    return PlayAnimation(Presets::AnimEvent::kItemPickup,a_actor,a_item);
}

int Manager::OnDrop(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kItemDrop,a_actor,a_item);
}

int Manager::OnItemAdd(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kItemAdd,a_actor,a_item);
}

int Manager::OnItemRemove(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kItemRemove,a_actor,a_item);
}

int Manager::OnEquip(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kEquip,a_actor,a_item);
}

int Manager::OnUnequip(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kUnequip,a_actor,a_item);
}

int Manager::OnBuy(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kBuy,a_actor,a_item);
}

int Manager::OnSell(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item)
{
    return PlayAnimation(Presets::AnimEvent::kSell,a_actor,a_item);
}

int Manager::OnMenuOpenClose(const std::string_view menu_name, const bool opened)
{
    const auto player = RE::PlayerCharacter::GetSingleton();
	const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, opened ? Presets::kOpen : Presets::kClose);
    return PlayAnimation(menuanimevent, player, nullptr);
}

int Manager::OnItemHover(const std::string_view menu_name, const RE::StandardItemData* a_item_data)
{
#undef GetObject
	const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, Presets::kHover);
    RE::TESObjectREFRPtr a_owner;

    if (RE::LookupReferenceByHandle(a_item_data->owner,a_owner)) {
        return PlayAnimation(menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject());
    }
    if (menuanimevent == Presets::AnimEvent::kMenuHoverBarter) {
		const auto handle = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>()->GetTargetRefHandle();
		if (RE::LookupReferenceByHandle(handle,a_owner)) {
            return PlayAnimation(menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject());
		}
	}
    return 0;
}

