#include "Presets/PresetInterface.h"
#include "Service.h"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersTXT.hpp"
#include <rapidjson/error/en.h>

namespace {
    template <typename T>
    bool CollectForms(const std::string& form_string, std::unordered_set<T*>& a_container) {
        std::shared_lock lock(PresetHelpers::formGroups_mutex_);
        if (const auto it = PresetHelpers::formGroups.find(form_string); it != PresetHelpers::formGroups.end()) {
            if (it->second.empty()) {
                logger::warn("Form group '{}' is empty.", form_string);
                return false;
            }
            for (auto a_formid : it->second) {
                if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                    a_container.insert(a_form);
                } else {
                    logger::warn("Failed to get form for string: {}", form_string);
                    return false;
                }
            }
        } else if (const auto a_formid = FormReader::GetFormEditorIDFromString(form_string); a_formid > 0) {
            if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                a_container.insert(a_form);
            } else {
                logger::warn("Failed to get form for string: {}", form_string);
                return false;
            }
        } else {
            logger::warn("Failed to get form ID for string: {}", form_string);
            return false;
        }
        return true;
    }

    bool CollectFormIDs(const std::string& form_string, std::unordered_set<RE::FormID>& a_container) {
        std::shared_lock lock(PresetHelpers::formGroups_mutex_);
        if (const auto it = PresetHelpers::formGroups.find(form_string); it != PresetHelpers::formGroups.end()) {
            if (it->second.empty()) {
                logger::warn("Form group '{}' is empty.", form_string);
                return false;
            }
            for (auto a_formid : it->second) {
                a_container.insert(a_formid);
            }
        } else if (const auto a_formid = FormReader::GetFormEditorIDFromString(form_string); a_formid > 0) {
            a_container.insert(a_formid);
        } else {
            logger::warn("Failed to get form ID for string: {}", form_string);
            return false;
        }
        return true;
    }

    // Returns true if the token is negated (starts with '!'), and sets 'out' to the token without the negation prefix.
    bool IsNegatedToken(const std::string& s, std::string_view& out) {
        if (!s.empty() && s.front() == '!') {
            out = std::string_view(s).substr(1);
            return true;
        }
        out = s;
        return false;
    }

    bool CollectFormType(const std::string& token, std::unordered_set<RE::FormType>& container) {
        if (const auto form_type = RE::StringToFormType(token);
            form_type < RE::FormType::Max && form_type > RE::FormType::None) {
            container.insert(form_type);
        } else {
            logger::warn("Invalid form type string: {}", token);
            return false;
        }
        return true;
    }

    template <typename T, typename F>
    bool CollectFilterTokens(const std::vector<std::string>& tokens, std::unordered_set<T>& includes,
                             std::unordered_set<T>& excludes, const F& collect) {
        for (const auto& value : tokens) {
            std::string_view token;
            if (auto& container = IsNegatedToken(value, token) ? excludes : includes;
                !collect(std::string(token), container)) {
                return false;
            }
        }
        return true;
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
            [](const std::string& token, auto& container) { return CollectForms(token, container); });
    }

    bool CollectFormIDFilters(const std::vector<std::string>& tokens, std::unordered_set<RE::FormID>& includes,
                              std::unordered_set<RE::FormID>& excludes) {
        return CollectFilterTokens(
            tokens,
            includes,
            excludes, [](const std::string& token, auto& container) {
                return CollectFormIDs(token, container);
            });
    }

    bool CollectFormTypeFilters(const std::vector<std::string>& tokens, std::unordered_set<RE::FormType>& includes,
                                std::unordered_set<RE::FormType>& excludes) {
        return CollectFilterTokens(
            tokens,
            includes,
            excludes,
            [](const std::string& token, auto& container) {
                return CollectFormType(token, container);
            });
    }
}

bool Presets::AnimData::TryLoad(AnimDataBlock& a_block) {
    constexpr auto formtype_index_max = static_cast<int>(RE::FormType::Max);
    constexpr auto formtype_index_min = static_cast<int>(RE::FormType::None);

    const auto actor_ids = a_block.actors.get();
    actors.insert(actor_ids.begin(), actor_ids.end());

    for (const auto numeric_form_types = a_block.form_types.get();
         const auto& form_type : numeric_form_types) {
        if (form_type < formtype_index_max && form_type > formtype_index_min) {
            form_types.insert(static_cast<RE::FormType>(form_type));
        } else {
            logger::warn("Invalid form type index: {}", form_type);
            return false;
        }
    }

    const bool filters_valid =
        CollectFormFilters(a_block.keywords.get(), keywords, exclude_keywords) &&
        CollectFormFilters(a_block.forms.get(), forms, exclude_forms) &&
        CollectFormFilters(a_block.locations.get(), locations, exclude_locations) &&
        CollectFormIDFilters(a_block.actors_str.get(), actors, exclude_actors) &&
        CollectFormFilters(a_block.actor_keywords.get(), actor_keywords, exclude_actor_keywords) &&
        CollectFormFilters(a_block.conditions.get(), conditions, exclude_conditions) &&
        CollectFormTypeFilters(a_block.form_types_str.get(), form_types, exclude_form_types);
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
        } else {
            logger::warn("Invalid event type index: {}", type);
            return false;
        }
    }

    if (const auto& type_custom = a_block.event_type_custom.get();
        !type_custom.empty()) {
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
                    logger::error("Failed to load preset; skipping file: {}", file.path().string());
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