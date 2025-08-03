#pragma once
#include <type_traits>
#include <vector>
#include <string>
#include <rapidjson/document.h>

namespace Presets {

    namespace detail {
        template <typename T>
        struct is_std_vector : std::false_type {};

        template <typename T, typename A>
        struct is_std_vector<std::vector<T, A>> : std::true_type {};

        template <typename T>
        inline constexpr bool is_std_vector_v = is_std_vector<T>::value;
    }

    namespace Getters {

        namespace JSON {
            template <typename T>
            bool Get(const rapidjson::Value& val, T& a_to_set) {
                if constexpr (detail::is_std_vector_v<T>) {
                    using ElemType = typename T::value_type;

                    if (!val.IsArray()) return false;

                    for (const auto& item : val.GetArray()) {
                        if constexpr (std::is_same_v<ElemType, std::string>) {
                            if (item.IsString()) a_to_set.push_back(item.GetString());
                            else return false;
                        } else if constexpr (std::is_same_v<ElemType, int>) {
                            if (item.IsInt()) a_to_set.push_back(item.GetInt());
                            else return false;
                        } else if constexpr (std::is_same_v<ElemType, bool>) {
                            if (item.IsBool()) a_to_set.push_back(item.GetBool());
                            else return false;
                        } else {
                            static_assert([]{ return false; }(), "Unsupported element type in vector");
                        }
                    }

                    return true;

                }
                else {
                    if constexpr (std::is_same_v<T, std::string>) {
                        if (val.IsString()) {
                            a_to_set = val.GetString();
                            return true;
                        }
                    } else if constexpr (std::is_same_v<T, int>) {
                        if (val.IsInt()) {
                            a_to_set = val.GetInt();
                            return true;
                        }
                    } else if constexpr (std::is_same_v<T, bool>) {
                        if (val.IsBool()) {
                            a_to_set = val.GetBool();
                            return true;
                        }
                    } else {
                        static_assert([]{ return false; }(), "Unsupported scalar type");
                    }

                    return false;
                }
            }

            template <typename T>
            bool Get(const rapidjson::Value& obj, const std::string& key, T& out) {
                if (!obj.IsObject() || !obj.HasMember(key.c_str())) return false;
                const auto& val = obj[key.c_str()];
                return Get(val, out);
            }


        }

        template <typename T, typename BlockType>
        bool GetValue(const BlockType& a_block, std::string& a_name, T& a_val) {
            T result;
            if constexpr (std::is_same_v<BlockType, rapidjson::Value>) {
                if (Presets::Getters::JSON::Get<T>(a_block,a_name,result)) {
                    a_val = result;
                    return true;
                }
            } else {
                // Add other types as needed
            }
            return false;
        }
    }

}