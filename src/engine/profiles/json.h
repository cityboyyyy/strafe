#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace engine::profiles::json
{
    struct member;

    enum class type
    {
        null,
        boolean,
        number,
        string,
        array,
        object
    };

    struct value
    {
        type kind = type::null;
        bool flag = false;
        double number = 0;
        bool integer = false;
        int integer_value = 0;
        std::string text;
        std::vector<value> items;
        std::vector<member> members;

        bool is_object() const { return kind == type::object; }
        bool is_array() const { return kind == type::array; }
        bool is_string() const { return kind == type::string; }
        bool is_bool() const { return kind == type::boolean; }
        bool is_int() const { return kind == type::number && integer; }
        bool is_number() const { return kind == type::number; }
        const value* find(std::string_view key) const;
        bool contains(std::string_view key) const { return find(key) != nullptr; }
    };

    struct member
    {
        std::string key;
        value data;
    };

    bool parse(std::string_view text, value& out, std::string& error, int depth_limit = 48);
    bool read_file(const std::filesystem::path& path, std::size_t maximum_bytes, std::string& text, std::string& error);
    std::string quote(std::string_view text);
}
