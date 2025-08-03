#pragma once
#include <shared_mutex>
#include <unordered_set>
#include "Config.h"
#include "boost/pfr/core.hpp"
#include "CLibUtilsQTR/FormReader.hpp"

namespace Presets {

    struct AnimDataBlock {
        Field<int,rapidjson::Value> priority = { "priority" };
        Field<std::vector<std::string>,rapidjson::Value> anim_names = { "names" };
        Field<std::vector<int>,rapidjson::Value> durations = { "durations"};
        Field<std::vector<int>,rapidjson::Value> event_type = { "types" };
        Field<std::vector<std::string>,rapidjson::Value> keywords = { "keywords" };
        Field<std::vector<std::string>,rapidjson::Value> locations = { "locations" };
        Field<std::vector<std::string>,rapidjson::Value> forms = { "forms" };
        Field<std::string,rapidjson::Value> attach_node = { "attach_node" };
        Field<std::vector<std::string>,rapidjson::Value> hide_nodes = { "hide_nodes" };

        void load(rapidjson::Value& a_block) {
            boost::pfr::for_each_field(*this, [&](auto& field) {
                field.load(a_block);
            });
        }
    };

    enum AnimEvent : std::uint8_t {
		kNone = 0,
        kActivate,
		kItemAdd,
		kItemRemove,
		kItemDrop,
		kItemPickup,
		kEquip,
		kUnequip,
		kBuy,
        kSell,
		kMenuOpenContainer,
		kMenuCloseContainer,
		kMenuOpenInventory,
		kMenuCloseInventory,
		kMenuOpenMagic,
		kMenuCloseMagic,
		kMenuOpenMap,
		kMenuCloseMap,
		kMenuOpenBarter,
		kMenuCloseBarter,
		kMenuOpenJournal,
		kMenuCloseJournal,
		kMenuHoverInventory,
		kMenuHoverMagic,
		kMenuHoverContainer,
		kMenuHoverBarter,
		kTotal
	};

    struct AnimData {
		std::vector<std::pair<std::string, int>> animations; // Animation Chain: <name, duration>
        int priority;
		std::unordered_set<AnimEvent> events;
		std::unordered_set<RE::BGSKeyword*> keywords;
		std::unordered_set<RE::TESForm*> forms;
		std::unordered_set<RE::FormType> form_types;
		std::unordered_set<RE::BGSLocation*> locations;
        std::string attach_node;
		std::vector<std::string> hide_nodes;

        explicit AnimData(AnimDataBlock& a_block);
    };

	inline std::shared_mutex m_anim_data_;
	inline std::unordered_map<AnimEvent,AnimData> anim_map;
    void Load();
}