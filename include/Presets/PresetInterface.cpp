#include "Presets/PresetInterface.h"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpers.hpp"


namespace  {
    template <typename T>
    void CollectForms(const std::string& form_string, std::unordered_set<T*>& a_container) {
        if (std::shared_lock lock(PresetHelpers::formGroups_mutex_); PresetHelpers::formGroups.contains(form_string)) {
            for (auto a_formid:PresetHelpers::formGroups.at(form_string)) {
                if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                    a_container.insert(a_form);
                }
                else {
                    logger::warn("Failed to get form for string: {}",form_string);
                }
            }
        }
        else if (const auto a_formid = FormReader::GetFormEditorIDFromString(form_string); a_formid >0) {
            if (auto a_form = RE::TESForm::LookupByID<T>(a_formid)) {
                a_container.insert(a_form);
            }
            else {
                logger::warn("Failed to get form for string: {}",form_string);
            }
        }

    }
}

Presets::AnimData::AnimData(AnimDataBlock& a_block) {
    priority = a_block.priority.get();

    const auto names = a_block.anim_names.get();
	auto durations = a_block.durations.get();

	size_t i = 0;
    for (const auto& name : names) {
        if (i < durations.size()) {
            animations.emplace_back(nullptr,name,durations[i]);
        } else {
            animations.emplace_back(nullptr,name,0);
        }
        ++i;
	}

	attach_node = a_block.attach_node.get();

    for (const auto& type : a_block.event_type.get()) {
        if (type < kTotal && type > kNone) {
            events.insert(static_cast<AnimEvent>(type));
        }
    }
    for (const auto& keyword : a_block.keywords.get()) {
        CollectForms(keyword,keywords);
    }
    for (const auto& form : a_block.forms.get()) {
        CollectForms(form,forms);
    }
    for (const auto& location : a_block.locations.get()) {
        CollectForms(location,locations);
    }

    for (const auto& a_formid : a_block.actors.get()) {
        actors.insert(a_formid);
    }
    for (const auto& a_formid_str : a_block.actors_str.get()) {
        if (const auto a_formid = FormReader::GetFormEditorIDFromString(a_formid_str); a_formid > 0) {
            actors.insert(a_formid);
        }
        else {
			logger::warn("Failed to get actor form for string: {}", a_formid_str);
        }
	}
    for (const auto& keyword : a_block.actor_keywords.get()) {
        CollectForms(keyword,actor_keywords);
    }

    for (const auto& node : a_block.hide_nodes.get()) {
        hide_nodes.push_back(node);
	}

    for (const auto& form_type : a_block.form_types.get()) {
        if (form_type < static_cast<int>(RE::FormType::Max) && form_type > static_cast<int>(RE::FormType::None)) {
            form_types.insert(static_cast<RE::FormType>(form_type));
        }
	}

    for (const auto& form_type_str : a_block.form_types_str.get()) {
        auto form_type = RE::StringToFormType(form_type_str);
        if (form_type < RE::FormType::Max && form_type > RE::FormType::None) {
            form_types.insert(form_type);
        }
	}

    delay = 0;

    if (const auto a_delay = a_block.delay_int.get(); a_delay > 0) {
        delay = a_delay;
    }
    else if (a_block.delay.get()) {
        int tot = 0;
        for (const auto dur : durations) {
            tot += dur;
        }
        if (tot > 0) {
            delay = tot;
        }
    }
}

Presets::AnimEvent Presets::GetMenuAnimEvent(const std::string_view menu_name, const MenuAnimEventType a_type)
{
    if (menu_name == RE::InventoryMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenInventory :
               a_type == kClose ? kMenuCloseInventory :
			a_type == kHover ? kMenuHoverInventory : kNone;
    }
    if (menu_name == RE::ContainerMenu::MENU_NAME) {
        return a_type == kOpen ? kMenuOpenContainer :
			a_type == kClose ? kMenuCloseContainer :
			a_type == kHover ? kMenuHoverContainer : kNone;
    }
    if (menu_name == RE::MagicMenu::MENU_NAME) {
		return a_type == kOpen ? kMenuOpenMagic :
			a_type == kClose ? kMenuCloseMagic : kNone;
    }
    if (menu_name == RE::FavoritesMenu::MENU_NAME) {
		return a_type == kOpen ? kMenuOpenFavorites :
			a_type == kClose ? kMenuCloseFavorites : kNone;
    }
    if (menu_name == RE::MapMenu::MENU_NAME) {
		return a_type == kOpen ? kMenuOpenMap :
			a_type == kClose ? kMenuCloseMap :kNone;
    }
    if (menu_name == RE::BarterMenu::MENU_NAME) {
		return a_type == kOpen ? kMenuOpenBarter :
			a_type == kClose ? kMenuCloseBarter :
			a_type == kHover ? kMenuHoverBarter : kNone;
    }
    if (menu_name == RE::JournalMenu::MENU_NAME) {
		return a_type == kOpen ? kMenuOpenJournal :
			a_type == kClose ? kMenuCloseJournal : kNone;
    }
    return kNone;
}

void Presets::Load() {
    constexpr std::string_view mod_folder = R"(Data\SKSE\Plugins\DAF)";

    if (!std::filesystem::exists(mod_folder)) {
        logger::error("Mod folder does not exist: {}", mod_folder);
        return;
    }

    // loop folder for folders
    for (const auto& entry : std::filesystem::directory_iterator(mod_folder)) {
        if (!entry.is_directory()) {
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
                    logger::error("JSON Parse Error at offset {}: {}", doc.GetErrorOffset(), doc.GetParseError());
                    continue;
                }
                AnimDataBlock data;
                data.load(doc);
                AnimData anim_data(data);

                for (std::unique_lock lock(m_anim_data_);
                    auto a_event_type : anim_data.events) {
                    anim_map[a_event_type].push_back(anim_data);
                }
            }
        }
    }
}