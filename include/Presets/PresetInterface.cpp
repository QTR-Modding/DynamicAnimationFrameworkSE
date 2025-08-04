#include "Presets/PresetInterface.h"

Presets::AnimData::AnimData(AnimDataBlock& a_block) {
    priority = a_block.priority.get();

    auto names = a_block.anim_names.get();
	auto durations = a_block.durations.get();

	size_t i = 0;
    for (const auto& name : names) {
        if (i < durations.size()) {
            animations.emplace_back(name, durations[i]);
        } else {
            animations.emplace_back(name, 0);
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
        auto a_formid = FormReader::GetFormEditorIDFromString(keyword);
        if (auto a_form = RE::TESForm::LookupByID<RE::BGSKeyword>(a_formid)) {
            keywords.insert(a_form);
        } else {
            logger::warn("Failed to find keyword: {}", keyword);
        }
    }
    for (const auto& form : a_block.forms.get()) {
        auto a_formid = FormReader::GetFormEditorIDFromString(form);
        if (auto a_form = RE::TESForm::LookupByID(a_formid)) {
            forms.insert(a_form);
        } else {
            logger::warn("Failed to find form: {}", form);
        }
    }
    for (const auto& location : a_block.locations.get()) {
        auto a_formid = FormReader::GetFormEditorIDFromString(location);
        if (auto a_form = RE::TESForm::LookupByID<RE::BGSLocation>(a_formid)) {
            locations.insert(a_form);
        }
        else {
            logger::warn("Failed to find location: {}", location);
        }
    }

    for (const auto& node : a_block.hide_nodes.get()) {
        hide_nodes.push_back(node);
	}

    for (const auto& form_type : a_block.form_types.get()) {
        if (form_type < static_cast<int>(RE::FormType::Max) && form_type > static_cast<int>(RE::FormType::None)) {
            form_types.insert(static_cast<RE::FormType>(form_type));
        }
	}
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
					anim_map.emplace(a_event_type, anim_data);
                }
            }
        }
    }
}