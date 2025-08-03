#pragma once
#include "Getters.h"

namespace Presets {

    template <typename T, typename BlockType>
	class Field {
		std::string name;
		T value;
	public:
		Field(const std::string& a_name) : name(a_name) {}

	    bool load(const BlockType& node);
        T& get() { return value; }
        const T& get() const { return value; }
	};

    template <typename T, typename BlockType>
    bool Field<T, BlockType>::load(const BlockType& node) {
        if (Getters::GetValue(node, name, value)) {
            return true;
        }
        return false;
    }
}