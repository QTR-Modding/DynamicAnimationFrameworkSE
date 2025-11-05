#include "Manager.h"
#include "Hooks.h"
#include "Utils.h"
#include "Animator.h"

namespace {
    // Credits: shad0wshayd3, https://next.nexusmods.com/profile/shad0wshayd3?gameId=1704
    void OpenJournalMenu(bool unk)
    {
        using func_t = decltype(&OpenJournalMenu);
        const REL::Relocation<func_t> func{ RELOCATION_ID(52428, 53327) };
        return func(unk);
    }
}

bool Manager::PlayAnimation(RE::Actor* a_actor, const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain)
{
    if (RE::ActorHandlePtr actor;
        RE::BSPointerHandleManagerInterface<RE::Actor>::GetSmartPointer(a_actor->GetHandle(), actor)) {
        MyAnimator* animator = nullptr;
        if (std::shared_lock lock(m_animators_); animators.contains(actor)) {
            animator = animators.at(actor);
        }
        if (!animator) {
            animator = new MyAnimator(actor);
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

int Manager::PlayAnimation(AnimEventInfo a_info)
{
    if (const auto actor = a_info.a_actor->As<RE::Actor>()) {
        if (!ActorCheck(actor)) {
            return 0;
        }

        if (const auto anim_data = GetAnimData(a_info.event_id,{.actor_id= actor->GetFormID(),.form= a_info.a_item}); 
            !anim_data.animations.empty()) {

            if (PlayAnimation(actor,{a_info.event_id,anim_data.animations})) {
                if (auto attach_node = anim_data.attach_node; !attach_node.empty()) {
                    if (const auto actor_id = actor->GetFormID(); !Hooks::item_meshes.contains(actor_id)) {
                        // ReSharper disable once CppTooWideScopeInitStatement
                        RE::NiPointer<RE::NiAVObject> a_model;
                        if (Utils::GetModel(a_info.a_item, a_model); a_model) {
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

void Manager::SetMenuQueued(const std::string_view menu_name, const bool for_open)
{
    queued_menus[menu_name] = for_open;
}

void Manager::UnSetMenuQueued(const std::string_view menu_name)
{
    queued_menus.erase(menu_name);
}

bool Manager::IsMenuQueued(const std::string_view menu_name) const
{
    return queued_menus.contains(menu_name);
}

bool Manager::IsMenuQueued(const std::string_view menu_name, const bool for_open) const
{
    return queued_menus.contains(menu_name) && queued_menus.at(menu_name) == for_open;
}

void Manager::OpenCloseMenu(const std::string_view menu_name, const bool open)
{
    const auto a_type = open ? RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kHide;

    if (menu_name == RE::JournalMenu::MENU_NAME) {
        if (open) {
            OpenJournalMenu(false);
            return;
        }
    }

    if (menu_name == RE::InventoryMenu::MENU_NAME) {
        if (!open) {
            RE::UIMessageQueue::GetSingleton()->AddMessage(
                        RE::TweenMenu::MENU_NAME, a_type,nullptr);
        }
    }

    RE::UIMessageQueue::GetSingleton()->AddMessage(
                    menu_name, a_type,nullptr);
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

Presets::AnimData Manager::GetAnimData(const DAF_API::AnimEventID a_animevent, const Filter& filter)
{
    const auto filter_actorid = filter.actor_id;
    const auto filter_actor = RE::TESForm::LookupByID<RE::Actor>(filter.actor_id);
    const auto filter_actor_kw = filter_actor ? filter_actor->As<RE::BGSKeywordForm>() : nullptr;
    RE::TESBoundObject* filter_base = filter.form ? filter.form->Is(RE::FormType::Reference) ? filter.form->AsReference()->GetBaseObject() : filter.form->As<RE::TESBoundObject>() : nullptr;
    const auto filter_form_kw = filter_base ? filter_base->As<RE::BGSKeywordForm>() : nullptr;
    const auto filter_formtype = filter_base ? filter_base->GetFormType() : RE::FormType::None;
    const auto filter_target = filter.form ? filter.form->AsReference() : nullptr;

    // Precompute keywords once
    std::vector<RE::BGSKeyword*> form_kws;
    if (filter_form_kw) {
        filter_form_kw->ForEachKeyword([&form_kws](RE::BGSKeyword* a_kw){ form_kws.push_back(a_kw); return RE::BSContainer::ForEachResult::kContinue; });
    }
    std::vector<RE::BGSKeyword*> actor_kws;
    if (filter_actor_kw) {
        filter_actor_kw->ForEachKeyword([&actor_kws](RE::BGSKeyword* a_kw){ actor_kws.push_back(a_kw); return RE::BSContainer::ForEachResult::kContinue; });
    }
    RE::BGSLocation* current_loc = filter_actor ? filter_actor->GetCurrentLocation() : nullptr;

    Presets::AnimData* best = nullptr;
    int best_priority = std::numeric_limits<int>::max();

    if (std::shared_lock lock(Presets::m_anim_data_); Presets::anim_map.contains(a_animevent)) {
        for (auto& anim_data : Presets::anim_map.at(a_animevent)) {
            if (!anim_data.form_types.empty() && !anim_data.form_types.contains(filter_formtype)) {
                continue;
            }
            if (!anim_data.exclude_form_types.empty() && anim_data.exclude_form_types.contains(filter_formtype)) {
                continue;
            }
            // actor include / exclude by ID
            if (!anim_data.actors.empty() && !anim_data.actors.contains(filter_actorid)) {
                continue;
            }
            if (!anim_data.exclude_actors.empty() && anim_data.exclude_actors.contains(filter_actorid)) {
                continue;
            }
            // forms include / exclude
            if (!anim_data.forms.empty() && !anim_data.forms.contains(filter_base)) {
                continue;
            }
            if (!anim_data.exclude_forms.empty() && anim_data.exclude_forms.contains(filter_base)) {
                continue;
            }
            // form keywords include / exclude
            if (!anim_data.keywords.empty()) {
                if (!std::ranges::any_of(form_kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.keywords.contains(a_kw);})){ 
                    continue;
                }
            }
            if (!anim_data.exclude_keywords.empty()) {
                if (std::ranges::any_of(form_kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.exclude_keywords.contains(a_kw);})){ 
                    continue;
                }
            }
            // actor keywords include / exclude
            if (!anim_data.actor_keywords.empty()) {
                if (!std::ranges::any_of(actor_kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.actor_keywords.contains(a_kw);})){ 
                    continue;
                }
            }
            if (!anim_data.exclude_actor_keywords.empty()) {
                if (std::ranges::any_of(actor_kws,[&anim_data](RE::BGSKeyword* a_kw){return anim_data.exclude_actor_keywords.contains(a_kw);})){ 
                    continue;
                }
            }

            // conditions include / exclude
            if (!anim_data.conditions.empty() && filter_actor &&
                !std::ranges::any_of(anim_data.conditions, [&filter_actor,&filter_target](const RE::BGSPerk* a_perk) {
                    return a_perk && a_perk->perkConditions.IsTrue(filter_actor,filter_target);
            })) {
                continue;
            }
            if (!anim_data.exclude_conditions.empty() && filter_actor &&
                std::ranges::any_of(anim_data.exclude_conditions, [&filter_actor,&filter_target](const RE::BGSPerk* a_perk) {
                    return a_perk && a_perk->perkConditions.IsTrue(filter_actor,filter_target);
            })) {
                continue;
            }

            // locations include / exclude
            if (!anim_data.locations.empty()) {
                if (!anim_data.locations.contains(current_loc)) {
                    continue;
                }
            }
            if (!anim_data.exclude_locations.empty()) {
                if (anim_data.exclude_locations.contains(current_loc)) {
                    continue;
                }
            }

            if (anim_data.priority < best_priority) {
                best_priority = anim_data.priority;
                best = &anim_data;
            }
        }
    }

    if (best) {
        return *best;
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

int Manager::OnActivate(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kActivate,a_actor,a_item});
}

int Manager::OnPickup(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kItemPickup,a_actor,a_item});
}

int Manager::OnDrop(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kItemDrop,a_actor,a_item});
}

int Manager::OnItemAdd(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kItemAdd,a_actor,a_item});
}

int Manager::OnItemRemove(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kItemRemove,a_actor,a_item});
}

int Manager::OnEquip(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kEquip,a_actor,a_item});
}

int Manager::OnUnequip(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kUnequip,a_actor,a_item});
}

int Manager::OnBuy(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kBuy,a_actor,a_item});
}

int Manager::OnSell(RE::TESObjectREFR* a_actor, RE::TESForm* a_item)
{
    return PlayAnimation({Presets::AnimEvent::kSell,a_actor,a_item});
}

int Manager::OnMenuOpenClose(const std::string_view menu_name, const bool opened)
{
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, opened ? Presets::kOpen : Presets::kClose);
    return PlayAnimation({menuanimevent, player, nullptr});
}

int Manager::OnItemHover(const std::string_view menu_name, const RE::StandardItemData* a_item_data)
{
#undef GetObject
    const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, Presets::kHover);
    RE::TESObjectREFRPtr a_owner;

    if (RE::LookupReferenceByHandle(a_item_data->owner,a_owner)) {
        return PlayAnimation({menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject()});
    }
    if (menuanimevent == Presets::AnimEvent::kMenuHoverBarter) {
        const auto handle = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>()->GetTargetRefHandle();
        if (RE::LookupReferenceByHandle(handle,a_owner)) {
            return PlayAnimation({menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject()});
        }
    }
    return 0;
}

