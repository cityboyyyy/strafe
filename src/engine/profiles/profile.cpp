#include "profile.h"

#include "../bindings/bindings.h"
#include "../input/controller.h"
#include "../output/output.h"
#include "../util/text.h"

#include <windows.h>

#include <algorithm>
#include <random>
#include <vector>

namespace engine::profiles
{
    namespace
    {
        bool try_input(const std::string& value, int version, input_binding& source)
        {
            source = {};
            constexpr std::string_view prefix = "Controller:";
            if (version == 1 || value.starts_with(prefix))
            {
                const std::string_view name = version == 1 ? std::string_view(value) : std::string_view(value).substr(prefix.size());
                const auto button = input::button_from_id(name);
                if (!button)
                    return false;
                source = bindings::controller(*button);
                return true;
            }

            output_action action = output_action::none;
            if (!output::try_parse(value, action))
                return false;
            if (action < output_action::mouse_left)
                source = { input_kind::keyboard, output::code(action) };
            else if (bindings::is_mouse_input(action))
                source = { input_kind::mouse, output::code(action) };
            else
                return false;
            return true;
        }

        bool try_binding(const json::value& value, int version, binding& result)
        {
            output_action target = output_action::none;
            if (version == 1 && value.is_string() && output::try_parse(value.text, target))
            {
                result = binding{ { target }, false };
                return true;
            }
            if (version != 2 || !value.is_object())
                return false;
            const json::value* list = value.find("targets");
            const json::value* mute = value.find("suppress");
            if (list == nullptr || !list->is_array() || list->items.empty() || list->items.size() > 8 ||
                mute == nullptr || !mute->is_bool())
                return false;
            std::vector<output_action> targets;
            for (const auto& item : list->items)
            {
                if (!item.is_string() || !output::try_parse(item.text, target) ||
                    std::find(targets.begin(), targets.end(), target) != targets.end())
                    return false;
                targets.push_back(target);
            }
            result = binding{ std::move(targets), mute->flag };
            return true;
        }

        std::string temp_name()
        {
            std::random_device device;
            static constexpr char digits[] = "0123456789abcdef";
            std::string name = ".padbindings-";
            for (int i = 0; i < 4; ++i)
            {
                const std::uint32_t chunk = device();
                for (int shift = 28; shift >= 0; shift -= 4)
                    name.push_back(digits[(chunk >> shift) & 15]);
            }
            return name + ".tmp";
        }

        bool write_all(const std::filesystem::path& path, const std::string& text, std::string& error)
        {
            HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                error = util::win32_message(GetLastError());
                return false;
            }
            DWORD written = 0;
            bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                written == text.size() && FlushFileBuffers(file);
            const DWORD code = GetLastError();
            ok = CloseHandle(file) && ok;
            if (!ok)
                error = util::win32_message(code);
            return ok;
        }
    }

    bool read_json(const std::filesystem::path& path, json::value& root, std::string& error)
    {
        std::string text;
        if (!json::read_file(path, maximum_bytes, text, error))
            return false;
        if (!json::parse(text, root, error))
            return false;
        if (!root.is_object())
        {
            error = "The profile must contain a JSON object.";
            return false;
        }
        return true;
    }

    bool load(const std::filesystem::path& path, binding_map& result, std::string& error)
    {
        json::value root;
        if (!read_json(path, root, error))
            return false;
        const json::value* version = root.find("version");
        const json::value* content = root.find("bindings");
        if (version == nullptr || !version->is_int() || (version->integer_value != 1 && version->integer_value != 2) ||
            content == nullptr || !content->is_object())
        {
            error = "This file is not a supported Pad Bindings profile.";
            return false;
        }

        const int number = version->integer_value;
        binding_map loaded;
        for (const auto& row : content->members)
        {
            input_binding source;
            binding value;
            if (!try_input(row.key, number, source) || !try_binding(row.data, number, value) || loaded.contains(source))
            {
                error = "Invalid or duplicate binding for '" + row.key + "'.";
                return false;
            }
            loaded.set(source, std::move(value));
        }
        result = std::move(loaded);
        return true;
    }

    std::string to_json(const binding_map& bindings)
    {
        std::vector<const binding_map::entry*> rows;
        for (const auto& row : bindings)
            rows.push_back(&row);
        std::stable_sort(rows.begin(), rows.end(), [](const binding_map::entry* a, const binding_map::entry* b)
        {
            if (a->source.kind != b->source.kind)
                return a->source.kind < b->source.kind;
            return a->source.code < b->source.code;
        });

        std::string text = "{\r\n  \"version\": 2,\r\n  \"bindings\": {";
        bool first_row = true;
        for (const auto* row : rows)
        {
            std::string name;
            if (row->source.kind == input_kind::controller)
            {
                const auto id = input::button_id(static_cast<control_button>(row->source.code));
                name = "Controller:" + (id ? std::string(*id) : std::to_string(row->source.code));
            }
            else
            {
                name = output::serialize(static_cast<output_action>(row->source.code));
            }
            text += first_row ? "\r\n" : ",\r\n";
            first_row = false;
            text += "    " + json::quote(name) + ": {\r\n      \"targets\": [";
            bool first_target = true;
            for (const output_action target : row->value.targets)
            {
                text += first_target ? "\r\n" : ",\r\n";
                first_target = false;
                text += "        " + json::quote(output::serialize(target));
            }
            text += first_target ? "]" : "\r\n      ]";
            text += ",\r\n      \"suppress\": ";
            text += row->value.suppress ? "true" : "false";
            text += "\r\n    }";
        }
        text += first_row ? "}" : "\r\n  }";
        text += "\r\n}";
        return text;
    }

    bool save(const std::filesystem::path& path, const binding_map& bindings, std::string& error)
    {
        std::error_code failure;
        const std::filesystem::path full = std::filesystem::absolute(path, failure);
        if (failure)
        {
            error = failure.message();
            return false;
        }
        const std::filesystem::path temp = full.parent_path() / temp_name();
        const std::string text = to_json(bindings);
        bool ok = write_all(temp, text, error);
        if (ok)
        {
            const DWORD attributes = GetFileAttributesW(full.c_str());
            const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
            if (exists)
                ok = ReplaceFileW(full.c_str(), temp.c_str(), nullptr, 0, nullptr, nullptr) != FALSE;
            else
                ok = MoveFileExW(temp.c_str(), full.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
            if (!ok)
                error = util::win32_message(GetLastError());
        }
        DeleteFileW(temp.c_str());
        return ok;
    }
}
