#pragma once
#include <shared_mutex>
#include <unordered_set>
#include "Config.h"
#include "boost/pfr/core.hpp"
#include "CLibUtilsQTR/FormReader.hpp"

namespace Presets {

    struct AnimDataBlock {
        Field<std::string,rapidjson::Value> anim_name = { "name" };
        Field<int,rapidjson::Value> priority = { "priority" };
        Field<int,rapidjson::Value> duration = { "duration" };
        Field<std::string,rapidjson::Value> attach_node = { "attach_node" };
        Field<std::vector<int>,rapidjson::Value> event_type = { "types" };
        Field<std::vector<std::string>,rapidjson::Value> keywords = { "keywords" };
        Field<std::vector<std::string>,rapidjson::Value> locations = { "locations" };
        Field<std::vector<std::string>,rapidjson::Value> forms = { "forms" };

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
		kTotal
	};

    struct AnimData {
        std::string name;
        int priority;
        int duration;
        std::string attach_node;
		std::unordered_set<AnimEvent> events;
		std::unordered_set<RE::BGSKeyword*> keywords;
		std::unordered_set<RE::TESForm*> forms;
		std::unordered_set<RE::BGSLocation*> locations;

        explicit AnimData(AnimDataBlock& a_block);
    };

	inline std::shared_mutex m_anim_data_;
	inline std::unordered_map<AnimEvent,AnimData> anim_map;
    void Load();
}