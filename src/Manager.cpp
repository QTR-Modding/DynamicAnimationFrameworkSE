#include "Manager.h"
#include "Hooks.h"
#include "PlayerInteraction.h"
#include "Utils.h"
#include "Animator.h"
#include "Variables/AnimationIntegration.h"

namespace {
    // Credits: shad0wshayd3, https://next.nexusmods.com/profile/shad0wshayd3?gameId=1704
    void OpenJournalMenu(bool unk) {
        using func_t = decltype(&OpenJournalMenu);
        const REL::Relocation<func_t> func{RELOCATION_ID(52428, 53327)};
        return func(unk);
    }

    void CollectKeywordsVec(const RE::BGSKeywordForm* kf, std::vector<RE::BGSKeyword*>& out) {
        if (!kf) return;
        kf->ForEachKeyword([&out](RE::BGSKeyword* a_kw) {
            out.push_back(a_kw);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    template <typename T>
    bool any_in_set(const std::unordered_set<T*>& s, const std::vector<T*>& v) {
        for (auto x : v) {
            if (s.contains(x)) return true;
        }
        return false;
    }

    void PrepareIdleAnimations(std::vector<Animation>& a_animations, RE::TESObjectREFR* a_target) {
        
        RE::ObjectRefHandle targetHandle{};
        if (a_target) {
            targetHandle = a_target->GetHandle();
        }

        for (auto& animation : a_animations) {
            animation.target = targetHandle;

            if (!animation.a_idle || !animation.a_idle->childIdles || animation.a_idle->childIdles->size() == 0) {
                continue;
            }

            auto existing = std::move(animation.before_play);

            animation.before_play = [existing = std::move(existing)](RE::Actor* a_actor,
                                                                     Animation& a_animation) mutable {
                if (existing && !existing(a_actor, a_animation)) {
                    return false;
                }

                const auto targetRef = a_animation.target.get().get();

                if (!Utils::ParentCheck(a_animation.a_idle, a_actor, targetRef)) {
                    return false;
                }

                std::vector<RE::TESIdleForm*> candidates;

                if (a_animation.a_idle->childIdles) {
                    std::unordered_set<RE::TESIdleForm*> seen;
                    for (const auto& childIdle : *a_animation.a_idle->childIdles) {
                        const auto child = childIdle ? childIdle->As<RE::TESIdleForm>() : nullptr;
                        Utils::CollectIdles(child, candidates, a_actor, targetRef, seen);
                    }
                }

                if (candidates.empty()) {
                    return false;
                }

                RE::TESIdleForm* pick;
                if (candidates.size() == 1) {
                    pick = candidates.front();
                } else {
                    thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
                    pick = candidates[dist(rng)];
                }

#ifndef NDEBUG
                logger::trace("Picked {:08X} {} from {} candidates", pick->GetFormID(), pick->GetFormEditorID(),
                              candidates.size());
#endif

                a_animation.a_idle = pick;
                return true;
            };
        }
    }
}

bool Manager::PlayAnimation(RE::Actor* a_actor,
                            const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain) {
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

int Manager::PlayAnimation(AnimEventInfo a_info) {
    const auto a_actor = a_info.a_actor->As<RE::Actor>();
    if (!a_actor) {
        return 0;
    }
    return PlayAnimation(a_info, SelectAnimation(a_info.event_id, a_actor, a_info.a_item));
}

int Manager::PlayAnimation(AnimEventInfo a_info, Presets::AnimData a_data) {
    if (a_data.animations.empty()) {
        return 0;
    }

    const auto a_actor = a_info.a_actor->As<RE::Actor>();
    const auto a_target = a_info.a_item ? a_info.a_item->AsReference() : nullptr;
    if (!a_data.variables.empty()) {
        Variables::PrepareAnimations(a_data.animations, std::move(a_data.variables),
                                     a_data.duration_indices, a_target);
    }

    PrepareIdleAnimations(a_data.animations, a_target);
    if (!PlayAnimation(a_actor, {a_info.event_id, std::move(a_data.animations)})) {
        return 0;
    }

    if (!a_data.attach_node.empty()) {
        if (const auto a_actor_id = a_actor->GetFormID(); !Hooks::item_meshes.contains(a_actor_id)) {
            RE::NiPointer<RE::NiAVObject> a_model;
            if (a_info.a_item && Utils::GetModel(a_info.a_item, a_model)) {
                Hooks::item_meshes[a_actor_id] = {a_model, a_data.attach_node};
            }
        }
    }
    return a_data.delay;
}

void Manager::SetMenuQueued(const std::string_view menu_name, const bool for_open) {
    queued_menus[menu_name] = for_open;
}

void Manager::UnSetMenuQueued(const std::string_view menu_name) {
    queued_menus.erase(menu_name);
}

bool Manager::IsMenuQueued(const std::string_view menu_name, const bool for_open) const {
    return queued_menus.contains(menu_name) && queued_menus.at(menu_name) == for_open;
}

void Manager::OpenCloseMenu(const std::string_view menu_name, const bool open) {
    const auto a_type = open ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide;

    if (menu_name == RE::JournalMenu::MENU_NAME) {
        if (open) {
            OpenJournalMenu(false);
            return;
        }
    }

    if (menu_name == RE::InventoryMenu::MENU_NAME) {
        if (!open) {
            RE::UIMessageQueue::GetSingleton()->AddMessage(
                RE::TweenMenu::MENU_NAME, a_type, nullptr);
        }
    }

    RE::UIMessageQueue::GetSingleton()->AddMessage(
        menu_name, a_type, nullptr);
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

Presets::AnimData Manager::GetAnimData(const DAF_API::AnimEventID a_animevent, const Filter& filter) {
    const auto filter_actorid = filter.actor_id;
    const auto filter_actor = RE::TESForm::LookupByID<RE::Actor>(filter.actor_id);
    const auto filter_actor_kw = filter_actor ? filter_actor->GetActorBase() : nullptr;
    const auto filter_target = filter.form ? filter.form->AsReference() : nullptr;
    RE::TESForm* filter_base = filter_target ? filter_target->GetBaseObject() : filter.form;
    const auto filter_form_kw = filter_base ? filter_base->As<RE::BGSKeywordForm>() : nullptr;
    const auto filter_formtype = filter_base ? filter_base->GetFormType() : RE::FormType::None;

    // Precompute keywords once
    std::vector<RE::BGSKeyword*> form_kws;
    CollectKeywordsVec(filter_form_kw, form_kws);
    std::vector<RE::BGSKeyword*> actor_kws;
    CollectKeywordsVec(filter_actor_kw, actor_kws);
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
            if (!anim_data.keywords.empty() && !any_in_set(anim_data.keywords, form_kws)) {
                continue;
            }
            if (!anim_data.exclude_keywords.empty() && any_in_set(anim_data.exclude_keywords, form_kws)) {
                continue;
            }
            // actor keywords include / exclude
            if (!anim_data.actor_keywords.empty() && !any_in_set(anim_data.actor_keywords, actor_kws)) {
                continue;
            }
            if (!anim_data.exclude_actor_keywords.empty() && any_in_set(anim_data.exclude_actor_keywords, actor_kws)) {
                continue;
            }

            // conditions include / exclude
            if (!anim_data.conditions.empty() && filter_actor &&
                !std::ranges::any_of(anim_data.conditions, [&filter_actor,&filter_target](const RE::BGSPerk* a_perk) {
                    return a_perk && a_perk->perkConditions.IsTrue(filter_actor, filter_target);
                })) {
                continue;
            }
            if (!anim_data.exclude_conditions.empty() && filter_actor &&
                std::ranges::any_of(anim_data.exclude_conditions,
                                    [&filter_actor,&filter_target](const RE::BGSPerk* a_perk) {
                                        return a_perk && a_perk->perkConditions.IsTrue(filter_actor, filter_target);
                                    })) {
                continue;
            }

            // locations include / exclude
            if (!anim_data.locations.empty() && !anim_data.locations.contains(current_loc)) {
                continue;
            }
            if (!anim_data.exclude_locations.empty() && anim_data.exclude_locations.contains(current_loc)) {
                continue;
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

void Manager::PauseAnimators() {
    std::unique_lock lock(m_animators_);
    for (const auto& animator : animators | std::views::values) {
        animator->Pause();
    }
}

void Manager::ResumeAnimators() {
    std::unique_lock lock(m_animators_);
    for (const auto& animator : animators | std::views::values) {
        animator->Resume();
    }
}

Presets::AnimData Manager::SelectAnimation(DAF_API::AnimEventID a_event, RE::Actor* a_actor,
                                          RE::TESForm* a_target) {
    return ActorCheck(a_actor) ? GetAnimData(a_event, {.actor_id = a_actor->GetFormID(), .form = a_target}) :
                                 Presets::AnimData{};
}

int Manager::OnDrop(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kItemDrop, a_actor, a_item});
}

int Manager::OnItemAdd(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kItemAdd, a_actor, a_item});
}

int Manager::OnItemRemove(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kItemRemove, a_actor, a_item});
}

int Manager::OnEquip(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kEquip, a_actor, a_item});
}

int Manager::OnUnequip(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kUnequip, a_actor, a_item});
}

int Manager::OnBuy(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kBuy, a_actor, a_item});
}

int Manager::OnSell(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kSell, a_actor, a_item});
}

int Manager::OnMagicEffectCast(RE::TESObjectREFR* a_actor, RE::TESForm* a_effect) {
    return PlayAnimation({Presets::AnimEvent::kMagicEffectCast, a_actor, a_effect});
}

int Manager::OnMagicEffectTarget(RE::TESObjectREFR* a_actor, RE::TESForm* a_effect) {
    return PlayAnimation({Presets::AnimEvent::kMagicEffectTarget, a_actor, a_effect});
}

int Manager::OnHarvest(RE::TESObjectREFR* a_actor, RE::TESForm* a_item) {
    return PlayAnimation({Presets::AnimEvent::kHarvest, a_actor, a_item});
}

int Manager::OnMenuOpenClose(const std::string_view menu_name, const bool opened) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, opened ? Presets::kOpen : Presets::kClose);
    return PlayAnimation({menuanimevent, player, nullptr});
}

int Manager::OnItemHover(const std::string_view menu_name, const RE::StandardItemData* a_item_data) {
    #undef GetObject
    const auto menuanimevent = Presets::GetMenuAnimEvent(menu_name, Presets::kHover);
    RE::TESObjectREFRPtr a_owner;

    if (RE::LookupReferenceByHandle(a_item_data->owner, a_owner)) {
        return PlayAnimation({menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject()});
    }
    if (menuanimevent == Presets::AnimEvent::kMenuHoverBarter) {
        const auto handle = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>()->GetTargetRefHandle();
        if (RE::LookupReferenceByHandle(handle, a_owner)) {
            return PlayAnimation({menuanimevent, a_owner.get(), a_item_data->objDesc->GetObject()});
        }
    }
    return 0;
}


std::uintptr_t Hooks::PlayerInteraction::GetHookedActivation(const RE::TESBoundObject& a_base) {
    const std::uintptr_t* a_vtable;
    std::memcpy(&a_vtable, &a_base, sizeof(a_vtable));
    const auto a_slot = a_vtable[activate_vfunc_slot];
    return [a_slot]<class... Forms>(std::tuple<Forms*...>*) {
        std::uintptr_t a_original = 0;
        (((a_original = ActivateHook<Forms>::GetOriginalAddress(a_slot)) != 0) || ...);
        return a_original;
    }(static_cast<ActivationForms*>(nullptr));
}

bool Hooks::PlayerInteraction::UsesNativePickup(const RE::TESBoundObject& a_base, std::uintptr_t a_hooked_activate) {
    const std::uintptr_t* a_vtable;
    std::memcpy(&a_vtable, &a_base, sizeof(a_vtable));
    const auto a_activate = a_hooked_activate ? a_hooked_activate : a_vtable[activate_vfunc_slot];
    const REL::Relocation<const std::uintptr_t*> bound_vtable{RE::TESBoundObject::VTABLE[0]};
    const auto default_activate = bound_vtable.get()[activate_vfunc_slot];
    const auto text = REL::Module::get().segment(REL::Segment::textx);
    if (a_activate < text.address() || a_activate - text.address() > text.size() - 5) {
        return false;
    }
    if (a_activate == default_activate) {
        return true;
    }

    if (const auto a_light = a_base.As<RE::TESObjectLIGH>()) {
        return a_light->CanBeCarried();
    }
    if (const auto a_book = a_base.As<RE::TESObjectBOOK>()) {
        return a_book->TeachesSpell() && a_book->CanBeTaken();
    }

    if (!a_base.As<RE::TESObjectWEAP>()) {
        return false;
    }
    const auto instruction = reinterpret_cast<const std::uint8_t*>(a_activate);
    if (*instruction != 0xE9) {
        return false;
    }
    std::int32_t a_displacement;
    std::memcpy(&a_displacement, instruction + 1, sizeof(a_displacement));
    return a_activate + 5 + a_displacement == default_activate;
}

void Hooks::PlayerInteraction::Install(SKSE::Trampoline& a_trampoline) {
    PlayerActivationHooks::Install(a_trampoline, Dispatch, Drain);
}

RE::TESBoundObject* Hooks::PlayerInteraction::GetDirectActivationBase(const RE::PlayerCharacter& a_player,
                                                                         RE::TESObjectREFR& a_target) {
    if (a_target.IsActivationBlocked() || a_target.extraList.HasType<RE::ExtraOpenCloseActivateRef>() ||
        a_player.HasPerkEntries(RE::BGSEntryPoint::ENTRY_POINTS::kActivate)) {
        return nullptr;
    }
    if (const auto a_flags = a_target.extraList.GetByType<RE::ExtraFlags>();
        a_flags && a_flags->flags.any(RE::ExtraFlags::Flag::kBlockPlayerActivate)) {
        return nullptr;
    }
    constexpr std::uint8_t requires_activation_parent = 1;
    if (const auto a_parents = a_target.extraList.GetByType<RE::ExtraActivateRef>();
        a_parents && (a_parents->activateFlags & requires_activation_parent)) {
        return nullptr;
    }

    const auto a_base = a_target.GetBaseObject();
    if (a_base && a_base->Is(RE::FormType::Door) && (a_target.formFlags & RE::TESObjectREFR::RecordFlags::kInaccessible)) {
        return nullptr;
    }
    return a_base;
}

bool Hooks::PlayerInteraction::Dispatch(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_target,
                                         RE::VR_DEVICE a_device) {
    const auto a_base = a_player ? GetDirectActivationBase(*a_player, *a_target) : nullptr;
    if (!a_base) {
        return false;
    }
    const auto a_hooked_activate = GetHookedActivation(*a_base);
    const bool uses_pickup = UsesNativePickup(*a_base, a_hooked_activate);
    if (!a_hooked_activate && !uses_pickup) {
        return false;
    }

    const auto a_action = PlayerActivationHooks::Capture(a_player, a_target, a_device);
    if (!a_action || !a_action->ReadyTarget()) {
        return false;
    }

    const Interaction interaction{*a_action};
    const auto manager = Manager::GetSingleton();
    auto a_activation = a_hooked_activate ? manager->SelectAnimation(Presets::kActivate, a_player, a_target) :
                                           Presets::AnimData{};
    if (!a_activation.animations.empty() && a_activation.delay > 0) {
        const auto delay = manager->PlayAnimation({Presets::kActivate, a_player, a_target}, std::move(a_activation));
        if (delay > 0) {
            CallDelayed(delay, [interaction] { interaction.StartPickup(); }, Submit);
            return true;
        }
        a_activation = {};
    }

    auto a_pickup = uses_pickup ? manager->SelectAnimation(Presets::kItemPickup, a_player, a_target) :
                                 Presets::AnimData{};
    if (!a_pickup.animations.empty() && a_pickup.delay > 0) {
        manager->PlayAnimation({Presets::kActivate, a_player, a_target}, std::move(a_activation));
        return interaction.StartPickup(&a_pickup);
    }
    return interaction.Replay(a_hooked_activate ? &a_activation : nullptr, uses_pickup ? &a_pickup : nullptr);
}

bool Hooks::PlayerInteraction::Interaction::StartPickup(Presets::AnimData* a_pickup) const {
    const auto a_target = action.ReadyTarget();
    if (!a_target) {
        return false;
    }

    Presets::AnimData a_activation_finished{};
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto a_base = GetDirectActivationBase(*player, *a_target);
    if (!a_base || !UsesNativePickup(*a_base, GetHookedActivation(*a_base))) {
        return Replay(&a_activation_finished, nullptr);
    }
    const auto manager = Manager::GetSingleton();
    auto a_selected = a_pickup ? std::move(*a_pickup) : manager->SelectAnimation(Presets::kItemPickup, player, a_target.get());
    if (!a_selected.animations.empty() && a_selected.delay > 0) {
        const auto delay = manager->PlayAnimation({Presets::kItemPickup, player, a_target.get()}, std::move(a_selected));
        a_selected = {};
        if (delay > 0) {
            CallDelayed(delay, [interaction = *this] {
                Presets::AnimData a_finished{};
                interaction.Replay(&a_finished, &a_finished);
            }, Submit);
            return true;
        }
    }
    return Replay(&a_activation_finished, &a_selected);
}

bool Hooks::PlayerInteraction::Interaction::Replay(Presets::AnimData* a_activation, Presets::AnimData* a_pickup) const {
    ReplayAnimations a_animations{a_activation, a_pickup};
    const auto a_previous = std::exchange(replaying, &a_animations);
    const SKSE::stl::scope_exit a_restore{[a_previous]() noexcept { replaying = a_previous; }};
    return action.Replay();
}

void Hooks::PlayerInteraction::Animate(DAF_API::AnimEventID a_event, RE::TESObjectREFR* a_actor,
                                      RE::TESObjectREFR* a_target) {
    const auto manager = Manager::GetSingleton();
    if (replaying && PlayerActivationHooks::IsReplaying(a_actor, a_target)) {
        auto& a_queued = a_event == Presets::kActivate ? replaying->activation : replaying->pickup;
        if (const auto a_animation = std::exchange(a_queued, nullptr)) {
            manager->PlayAnimation({a_event, a_actor, a_target}, std::move(*a_animation));
            return;
        }
    }
    manager->PlayAnimation({a_event, a_actor, a_target});
}

void Hooks::PlayerInteraction::Submit(SKSE::TaskInterface::TaskFn a_call) {
    std::lock_guard lock(ready_mutex);
    ready_calls.push_back(std::move(a_call));
}

void Hooks::PlayerInteraction::Drain() {
    std::vector<SKSE::TaskInterface::TaskFn> a_calls;
    {
        std::lock_guard lock(ready_mutex);
        a_calls.swap(ready_calls);
    }
    for (const auto& a_call : a_calls) {
        a_call();
    }
}
