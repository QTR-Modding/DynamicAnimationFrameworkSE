#pragma once
#include <mutex>
#include <unordered_set>

namespace Utils {
    inline std::mutex menu_blocks_mutex;
    inline std::map<std::string_view, bool> menu_blocks = {
        {RE::InventoryMenu::MENU_NAME, true},
        {RE::ContainerMenu::MENU_NAME, true},
        {RE::BarterMenu::MENU_NAME, true},
    };

    template <typename MenuType>
    RE::StandardItemData* GetSelectedItemData();

    RE::StandardItemData* GetSelectedItemDataInMenu(std::string& a_menuOut);

    const char* GetModelPath(RE::TESForm* a_form, RE::Actor* a_actor = nullptr);
    bool GetModel(RE::TESForm* a_form, RE::NiPointer<RE::NiAVObject>& a_out);

    bool ShouldSkip(const RE::TESConditionItem* it);

    bool EvalConditionsFiltered(const RE::TESCondition& conds, RE::Actor* actor, RE::TESObjectREFR* target);

    bool ParentCheck(const RE::TESIdleForm* idle, RE::Actor* actor, RE::TESObjectREFR* target);

    void CollectIdles(RE::TESIdleForm* parent_idle, std::vector<RE::TESIdleForm*>& out, RE::Actor* actor,
                      RE::TESObjectREFR* target, std::unordered_set<RE::TESIdleForm*>& seen);
}

namespace ModCompatibility {
    struct ModInfo {
        explicit ModInfo(std::string a_modName) : name(std::move(a_modName)) {
        }

        bool IsInstalled();

    private:
        std::string name;
        bool isLoaded = false;
        bool is_checked = false;
    };

    inline auto QuestJournalOverhaul = ModInfo("QuestJournalOverhaul");
}