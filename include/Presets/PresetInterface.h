#pragma once
#include <shared_mutex>
#include <unordered_set>
#include "boost/pfr/core.hpp"
#include "CLibUtilsQTR/FormReader.hpp"
#include "CLibUtilsQTR/PresetHelpers/Config.hpp"
#include "DynamicAnimationFramework/API.hpp"
#include "Animator.h"


namespace Presets {

    struct AnimDataBlock {
        Field<int,rapidjson::Value> priority = { "priority",0};
        Field<std::vector<int>,rapidjson::Value> event_type = { "events" };
        Field<std::string,rapidjson::Value> event_type_custom = { "events" };

        Field<std::vector<std::string>,rapidjson::Value> anim_names = { "animations" };
        Field<std::vector<int>,rapidjson::Value> durations = { "durations"};

        Field<std::vector<std::string>,rapidjson::Value> forms = { "forms" };
        Field<std::vector<int>,rapidjson::Value> form_types = { "form_types" };
        Field<std::vector<std::string>,rapidjson::Value> form_types_str = { "form_types" };
        Field<std::vector<std::string>,rapidjson::Value> keywords = { "keywords" };

        Field<std::vector<uint32_t>,rapidjson::Value> actors = { "actors" };
        Field<std::vector<std::string>,rapidjson::Value> actors_str = { "actors" };
        Field<std::vector<std::string>,rapidjson::Value> locations = { "locations" };
		Field<std::vector<std::string>,rapidjson::Value> actor_keywords = { "actor_keywords" };

        Field<std::string,rapidjson::Value> attach_node = { "attach_node" };
        Field<std::vector<std::string>,rapidjson::Value> hide_nodes = { "hide_nodes" };

        Field<bool,rapidjson::Value> delay = { "delay",false};
        Field<int,rapidjson::Value> delay_int = { "delay",0};

        void load(rapidjson::Value& a_block) {
            boost::pfr::for_each_field(*this, [&](auto& field) {
                field.load(a_block);
            });
        }
    };

    enum AnimEvent : DAF_API::AnimEventID {
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
		kMenuOpenInventory,
		kMenuCloseInventory,
		kMenuHoverInventory,
		kMenuOpenContainer,
		kMenuCloseContainer,
		kMenuHoverContainer,
		kMenuOpenMagic,
		kMenuCloseMagic,
		//kMenuHoverMagic,
		kMenuOpenBarter,
		kMenuCloseBarter,
		kMenuHoverBarter,
		kMenuOpenFavorites,
		kMenuCloseFavorites,
		kMenuOpenMap,
		kMenuCloseMap,
		kMenuOpenJournal,
		kMenuCloseJournal,
		//kPOVToggle,
		kTotal
	};

	enum MenuAnimEventType : std::uint8_t {
	    kOpen,
	    kClose,
	    kHover,
	};

	AnimEvent GetMenuAnimEvent(std::string_view menu_name, MenuAnimEventType a_type);

    struct AnimData {
		std::vector<Animation> animations;

        int priority;
		std::unordered_set<DAF_API::AnimEventID> events;

        std::unordered_set<RE::TESForm*> forms;
		std::unordered_set<RE::FormType> form_types;
		std::unordered_set<RE::BGSKeyword*> keywords;

        std::unordered_set<RE::FormID> actors;
		std::unordered_set<RE::BGSKeyword*> actor_keywords;
		std::unordered_set<RE::BGSLocation*> locations;

        std::string attach_node;
		std::vector<std::string> hide_nodes; // TODO: implement

		int delay;

        AnimData() = default;
        explicit AnimData(AnimDataBlock& a_block);
    };

	inline std::shared_mutex m_anim_data_;
	inline std::unordered_map<DAF_API::AnimEventID,std::vector<AnimData>> anim_map;
    void Load();
}