#pragma once

namespace Hooks {
    void Install();

    inline constinit std::atomic_uint64_t delayed_call_generation{0};

    using DelayedCallDispatcher = void (*)(SKSE::TaskInterface::TaskFn);

    void CallDelayed(int a_delay, SKSE::TaskInterface::TaskFn a_task, DelayedCallDispatcher a_dispatch = nullptr);
    void CancelDelayedCalls();

    struct DrawHook {
        static void thunk(std::uint32_t a_timer);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct WndProcHook {
        static LRESULT thunk(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam);
        static inline WNDPROC func;
        static void Install();
    };

    // Source: https://github.com/RavenKZP/Immersive-Weapon-Switch/blob/main/src/Hooks.cpp
    struct GenericEquipObjectHook {
        static void InstallHook(SKSE::Trampoline& a_trampoline);
        static void thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,
                          std::uint64_t a_unk);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct UnEquipObjectHook {
        static void InstallHook(SKSE::Trampoline& a_trampoline);
        static void thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,
                          std::uint64_t a_unk);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    inline constexpr std::size_t activate_vfunc_slot = 0x37;
    std::uintptr_t GetHookedActivation(const RE::TESBoundObject& a_base);

    using ActivationForms = std::tuple<
        RE::TESObjectARMO*, RE::TESObjectWEAP*, RE::ScrollItem*, RE::AlchemyItem*, RE::IngredientItem*,
        RE::TESObjectBOOK*, RE::TESObjectCONT*, RE::TESObjectDOOR*, RE::TESObjectSTAT*, RE::TESObjectACTI*,
        RE::TESObjectLIGH*, RE::TESObjectTREE*, RE::BGSTalkingActivator*, RE::TESNPC*>;

    template <typename FormType>
    class ActivateHook : public FormType {
        static bool Activate_Hook(FormType* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef,
                                  std::uint8_t a_arg3, RE::TESBoundObject* a_obj, std::int32_t a_targetCount);
        static inline REL::Relocation<decltype(&FormType::Activate)> _Activate;

    public:
        static void install();
        static std::uintptr_t GetOriginalAddress(std::uintptr_t a_slot);
    };

    // Credits: SkyrimThiago
    struct InventoryHoverHook {
        static int64_t thunk(RE::InventoryEntryData* a1);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    template <typename MenuType>
    class MenuHook : public MenuType {
        using ProcessMessage_t = decltype(&MenuType::ProcessMessage);
        static inline REL::Relocation<ProcessMessage_t> _ProcessMessage;
        RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::UIMessage& a_message);

    public:
        static void InstallHook(const REL::VariantID& varID);
    };

    template <typename RefType>
    class MoveItemHooks {
    public:
        static void install(bool is_actor = true);

    private:
        static void pickUpObject(RefType* a_this,
                                 RE::TESObjectREFR* a_object,
                                 int32_t a_count,
                                 bool a_arg3,
                                 bool a_play_sound);
        static inline REL::Relocation<decltype(pickUpObject)> pick_up_object_;

        static RE::ObjectRefHandle* RemoveItem(RefType* a_this,
                                               RE::ObjectRefHandle& a_hidden_return_argument,
                                               RE::TESBoundObject* a_item,
                                               std::int32_t a_count,
                                               RE::ITEM_REMOVE_REASON a_reason,
                                               RE::ExtraDataList* a_extra_list,
                                               RE::TESObjectREFR* a_move_to_ref,
                                               const RE::NiPoint3* a_drop_loc,
                                               const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(RemoveItem)> remove_item_;

        static void addObjectToContainer(RefType* a_this,
                                         RE::TESBoundObject* a_object,
                                         RE::ExtraDataList* a_extraList,
                                         std::int32_t a_count,
                                         RE::TESObjectREFR* a_fromRefr
            );
        static inline REL::Relocation<decltype(addObjectToContainer)> add_object_to_container_;
    };

    class MagicEffectHooks {
    public:
        static void Install();

    private:
        static void DispatchEffectAnimation(RE::ActiveEffect* a_effect);

        static void ValueModifierEffectCast(RE::ValueModifierEffect* a_this);
        static inline REL::Relocation<decltype(ValueModifierEffectCast)> _value_modifier_effect;

        static void PeakValueModifierEffectCast(RE::PeakValueModifierEffect* a_this);
        static inline REL::Relocation<decltype(PeakValueModifierEffectCast)> _peak_value_modifier_effect;

        static void ScriptEffectCast(RE::ScriptEffect* a_this);
        static inline REL::Relocation<decltype(ScriptEffectCast)> _script_effect;

        static void CalmEffectCast(RE::CalmEffect* a_this);
        static inline REL::Relocation<decltype(CalmEffectCast)> _calm_effect;
    };

    class AnimObjectHook {
    public:
        static RE::NiAVObject* thunk(RE::TESModel* a_model, RE::BIPED_OBJECT a_bipedObj,
                                     RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_biped,
                                     RE::NiAVObject* a_root);

        static inline REL::Relocation<decltype(thunk)> _LoadAnimObject;
    };

    using AttachNodeInfo = std::pair<RE::NiPointer<RE::NiAVObject>, std::string>;
    inline std::unordered_map<RE::FormID, AttachNodeInfo> item_meshes;

    static void add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4,
                                 bool a5);
    static inline REL::Relocation<decltype(add_item_functor)> add_item_functor_;
};