#include "Presets/PresetInterface.h"
#include "Service.h"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersTXT.hpp"
#include <rapidjson/error/en.h>

namespace {
    template <typename T>
    void CollectForms(const std::string& form_string, std::unordered_set<T*>& a_container) {
        if (std::shared_lock lock(PresetHelpers::formGroups_mutex_); PresetHelpers::formGroups.contains(form_string)) {
            for (auto a_formid : PresetHelpers::formGroups.at(form_string)) {
                if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                    a_container.insert(a_form);
                } else {
                    logger::warn("Failed to get form for string: {}", form_string);
                }
            }
        } else if (const auto a_formid = FormReader::GetFormEditorIDFromString(form_string); a_formid > 0) {
            if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                a_container.insert(a_form);
            } else {
                logger::warn("Failed to get form for string: {}", form_string);
            }
        }
    }

    bool IsNegatedToken(const std::string& s, std::string_view& out) {
        if (!s.empty() && s.front() == '!') {
            out = std::string_view(s).substr(1);
            return true;
        }
        out = s;
        return false;
    }

    void CollectActorFormID(const std::string& token, std::unordered_set<RE::FormID>& container) {
        if (const auto id = FormReader::GetFormEditorIDFromString(token); id > 0) {
            container.insert(id);
        } else {
            logger::warn("Failed to get actor form for string: {}", token);
        }
    }

    void CollectFormType(const std::string& token, std::unordered_set<RE::FormType>& container) {
        const auto form_type = RE::StringToFormType(token);
        if (form_type < RE::FormType::Max && form_type > RE::FormType::None) {
            container.insert(form_type);
        }
    }

    template <typename T, typename F>
    bool CollectFilterTokens(
        const std::vector<std::string>& tokens,
        std::unordered_set<T>& includes,
        std::unordered_set<T>& excludes,
        const F& collect,
        bool has_include = false) {
        for (const auto& s : tokens) {
            if (std::string_view tok; IsNegatedToken(s, tok)) {
                collect(std::string(tok), excludes);
            } else {
                has_include = true;
                collect(std::string(tok), includes);
            }
        }
        // Omitted and exclusion-only filters are valid; an include needs at least one resolved value.
        return !has_include || !includes.empty();
    }

    template <typename T>
    bool CollectFormFilters(
        const std::vector<std::string>& tokens,
        std::unordered_set<T*>& includes,
        std::unordered_set<T*>& excludes) {
        return CollectFilterTokens(
            tokens,
            includes,
            excludes,
            [](const std::string& token, auto& container) { CollectForms(token, container); });
    }
}

bool Presets::AnimData::TryLoad(AnimDataBlock& a_block) {
    const auto actor_ids = a_block.actors.get();
    actors.insert(actor_ids.begin(), actor_ids.end());

    const auto numeric_form_types = a_block.form_types.get();
    for (const auto& form_type : numeric_form_types) {
        if (form_type < static_cast<int>(RE::FormType::Max) && form_type > static_cast<int>(RE::FormType::None)) {
            form_types.insert(static_cast<RE::FormType>(form_type));
        }
    }

    const bool filters_valid =
        CollectFormFilters(a_block.keywords.get(), keywords, exclude_keywords) &&
        CollectFormFilters(a_block.forms.get(), forms, exclude_forms) &&
        CollectFormFilters(a_block.locations.get(), locations, exclude_locations) &&
        CollectFilterTokens(
            a_block.actors_str.get(), actors, exclude_actors, CollectActorFormID, !actor_ids.empty()) &&
        CollectFormFilters(a_block.actor_keywords.get(), actor_keywords, exclude_actor_keywords) &&
        CollectFormFilters(a_block.conditions.get(), conditions, exclude_conditions) &&
        CollectFilterTokens(
            a_block.form_types_str.get(),
            form_types,
            exclude_form_types,
            CollectFormType,
            !numeric_form_types.empty());
    if (!filters_valid) {
        return false;
    }

    priority = a_block.priority.get();

    const auto names = a_block.anim_names.get();
    auto durations = a_block.durations.get();

    size_t i = 0;
    for (const auto& name : names) {
        RE::TESIdleForm* a_idle = nullptr;
        //if (const auto idle_formid = FormReader::GetFormEditorIDFromString(name); idle_formid > 0){
        //    a_idle = RE::TESForm::LookupByID<RE::TESIdleForm>(idle_formid);
        //}
        if (i < durations.size()) {
            animations.emplace_back(a_idle, a_idle ? "" : name, durations[i]);
        } else {
            animations.emplace_back(a_idle, a_idle ? "" : name, 0);
        }
        ++i;
    }

    attach_node = a_block.attach_node.get();

    for (const auto& type : a_block.event_type.get()) {
        if (type < kTotal && type > kNone) {
            events.insert(type);
        }
    }

    if (const auto& type_custom = a_block.event_type_custom.get(); !type_custom.empty()) {
        const auto a_eventid = Service::AddCustomEvent(a_block.event_type_custom.get());
        events.insert(a_eventid);
    }

    for (const auto& node : a_block.hide_nodes.get()) {
        hide_nodes.push_back(node);
    }

    delay = 0;

    if (const auto a_delay = a_block.delay_int.get(); a_delay > 0) {
        delay = a_delay;
    } else if (a_block.delay.get()) {
        int tot = 0;
        for (const auto dur : durations) {
            tot += dur;
        }
        if (tot > 0) {
            delay = tot;
        }
    }
    return true;
}

Presets::AnimEvent Presets::GetMenuAnimEvent(const std::string_view menu_name, const MenuAnimEventType a_type) {
    if (menu_name == RE::InventoryMenu::MENU_NAME) {
        return a_type == kOpen
                   ? kMenuOpenInventory
                   : a_type == kClose
                   ? kMenuCloseInventory
                   : a_type == kHover
                   ? kMenuHoverInventory
                   : kNone;
    }
    if (menu_name == RE::ContainerMenu::MENU_NAME) {
        return a_type == kOpen
                   ? kMenuOpenContainer
                   : a_type == kClose
                   ? kMenuCloseContainer
                   : a_type == kHover
                   ? kMenuHoverContainer
                   : kNone;
    }
    if (menu_name == RE::MagicMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenMagic : a_type == kClose ? kMenuCloseMagic : kNone;
    }
    if (menu_name == RE::FavoritesMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenFavorites : a_type == kClose ? kMenuCloseFavorites : kNone;
    }
    if (menu_name == RE::MapMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenMap : a_type == kClose ? kMenuCloseMap : kNone;
    }
    if (menu_name == RE::BarterMenu::MENU_NAME) {
        return a_type == kOpen
                   ? kMenuOpenBarter
                   : a_type == kClose
                   ? kMenuCloseBarter
                   : a_type == kHover
                   ? kMenuHoverBarter
                   : kNone;
    }
    if (menu_name == RE::JournalMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenJournal : a_type == kClose ? kMenuCloseJournal : kNone;
    }
    return kNone;
}

void Presets::Load() {
    if (loaded) {
        return;
    }

    constexpr std::string_view animDataFolder = R"(Data\SKSE\Plugins\DAF\animData)";
    constexpr std::string_view formGroupsFolder = R"(Data\SKSE\Plugins\DAF\formGroups)";

    if (!std::filesystem::exists(animDataFolder)) {
        logger::error("Mod folder does not exist: {}", animDataFolder);
        return;
    }

    PresetHelpers::TXT_Helpers::GatherForms(std::string(formGroupsFolder));

    // loop folder for folders
    for (const auto& entry : std::filesystem::directory_iterator(animDataFolder)) {
        if (!entry.is_directory()) {
            continue;
        }
        // skip if it has special characters
        if (entry.path().filename().string().find_first_of("!@#$%^&*()[]{};:'\"\\|,.<>/?") != std::string::npos) {
            logger::warn("Skipping folder with special characters: {}", entry.path().filename().string());
            continue;
        }

        std::string folder_name = entry.path().filename().string();
        logger::info("Found folder: {}", folder_name);
        // Load JSON files in the folder
        for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
            if (file.path().extension() == ".json") {
                logger::info("Found JSON file: {}", file.path().filename().string());
                rapidjson::Document doc;
                // Load the JSON file
                std::ifstream ifs(file.path());
                if (!ifs.is_open()) {
                    logger::error("Failed to open file: {}", file.path().string());
                    continue;
                }
                std::string json_str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                ifs.close();
                doc.Parse(json_str.c_str());
                if (doc.HasParseError()) {
                    logger::error("JSON Parse Error at offset {}: {}", doc.GetErrorOffset(),
                                  rapidjson::GetParseError_En(doc.GetParseError()));
                    continue;
                }
                AnimDataBlock data;
                data.load(doc);
                AnimData anim_data;

                if (!anim_data.TryLoad(data)) {
                    logger::warn("Skipping preset with an unresolved include filter: {}", file.path().string());
                    continue;
                }

                for (std::unique_lock lock(m_anim_data_);
                     auto a_event_type : anim_data.events) {
                    anim_map[a_event_type].push_back(anim_data);
                }
            }
        }
    }

    loaded = true;
}