#include "json.h"

#include "../util/text.h"

#include <windows.h>

#include <charconv>
#include <cstdint>
#include <limits>

namespace engine::profiles::json
{
    namespace
    {
        class parser
        {
        public:
            parser(std::string_view text, int depth_limit)
                : text_(text)
                , depth_limit_(depth_limit)
            {
            }

            bool run(value& out, std::string& error)
            {
                skip_space();
                if (!parse_value(out, 0))
                {
                    error = message();
                    return false;
                }
                skip_space();
                if (position_ != text_.size())
                {
                    fail("Unexpected content after the JSON value");
                    error = message();
                    return false;
                }
                return true;
            }

        private:
            bool fail(const char* reason)
            {
                if (reason_ == nullptr)
                {
                    reason_ = reason;
                    failed_at_ = position_;
                }
                return false;
            }

            std::string message() const
            {
                std::size_t line = 1;
                std::size_t column = 1;
                for (std::size_t i = 0; i < failed_at_ && i < text_.size(); ++i)
                {
                    if (text_[i] == '\n')
                    {
                        ++line;
                        column = 1;
                    }
                    else
                    {
                        ++column;
                    }
                }
                return std::string(reason_ ? reason_ : "Invalid JSON") + " at line " + std::to_string(line) +
                    ", column " + std::to_string(column) + ".";
            }

            void skip_space()
            {
                while (position_ < text_.size())
                {
                    const char c = text_[position_];
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                        break;
                    ++position_;
                }
            }

            bool literal(std::string_view word)
            {
                if (text_.substr(position_, word.size()) != word)
                    return fail("Invalid JSON literal");
                position_ += word.size();
                return true;
            }

            bool parse_value(value& out, int depth)
            {
                if (position_ >= text_.size())
                    return fail("Unexpected end of JSON");
                const char c = text_[position_];
                switch (c)
                {
                case '{':
                    return parse_object(out, depth + 1);
                case '[':
                    return parse_array(out, depth + 1);
                case '"':
                    out.kind = type::string;
                    return parse_string(out.text);
                case 't':
                    out.kind = type::boolean;
                    out.flag = true;
                    return literal("true");
                case 'f':
                    out.kind = type::boolean;
                    out.flag = false;
                    return literal("false");
                case 'n':
                    out.kind = type::null;
                    return literal("null");
                default:
                    if (c == '-' || (c >= '0' && c <= '9'))
                        return parse_number(out);
                    return fail("Invalid JSON value");
                }
            }

            bool parse_object(value& out, int depth)
            {
                if (depth > depth_limit_)
                    return fail("JSON nesting is too deep");
                out.kind = type::object;
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == '}')
                {
                    ++position_;
                    return true;
                }
                for (;;)
                {
                    skip_space();
                    if (position_ >= text_.size() || text_[position_] != '"')
                        return fail("Expected a property name");
                    std::string key;
                    if (!parse_string(key))
                        return false;
                    skip_space();
                    if (position_ >= text_.size() || text_[position_] != ':')
                        return fail("Expected ':'");
                    ++position_;
                    skip_space();
                    value item;
                    if (!parse_value(item, depth))
                        return false;
                    bool replaced = false;
                    for (auto& existing : out.members)
                    {
                        if (existing.key == key)
                        {
                            existing.data = std::move(item);
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced)
                        out.members.push_back({ std::move(key), std::move(item) });
                    skip_space();
                    if (position_ >= text_.size())
                        return fail("Unexpected end of JSON");
                    if (text_[position_] == ',')
                    {
                        ++position_;
                        continue;
                    }
                    if (text_[position_] == '}')
                    {
                        ++position_;
                        return true;
                    }
                    return fail("Expected ',' or '}'");
                }
            }

            bool parse_array(value& out, int depth)
            {
                if (depth > depth_limit_)
                    return fail("JSON nesting is too deep");
                out.kind = type::array;
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == ']')
                {
                    ++position_;
                    return true;
                }
                for (;;)
                {
                    skip_space();
                    value item;
                    if (!parse_value(item, depth))
                        return false;
                    out.items.push_back(std::move(item));
                    skip_space();
                    if (position_ >= text_.size())
                        return fail("Unexpected end of JSON");
                    if (text_[position_] == ',')
                    {
                        ++position_;
                        continue;
                    }
                    if (text_[position_] == ']')
                    {
                        ++position_;
                        return true;
                    }
                    return fail("Expected ',' or ']'");
                }
            }

            bool hex4(std::uint32_t& code)
            {
                if (position_ + 4 > text_.size())
                    return fail("Invalid unicode escape");
                code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char c = text_[position_++];
                    code <<= 4;
                    if (c >= '0' && c <= '9')
                        code |= static_cast<std::uint32_t>(c - '0');
                    else if (c >= 'a' && c <= 'f')
                        code |= static_cast<std::uint32_t>(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F')
                        code |= static_cast<std::uint32_t>(c - 'A' + 10);
                    else
                        return fail("Invalid unicode escape");
                }
                return true;
            }

            static void append_utf8(std::string& out, std::uint32_t code)
            {
                if (code < 0x80)
                {
                    out.push_back(static_cast<char>(code));
                }
                else if (code < 0x800)
                {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else if (code < 0x10000)
                {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else
                {
                    out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
            }

            bool parse_string(std::string& out)
            {
                ++position_;
                out.clear();
                while (position_ < text_.size())
                {
                    const char c = text_[position_++];
                    if (c == '"')
                        return true;
                    if (c != '\\')
                    {
                        out.push_back(c);
                        continue;
                    }
                    if (position_ >= text_.size())
                        break;
                    const char escape = text_[position_++];
                    switch (escape)
                    {
                    case '"':
                    case '\\':
                    case '/':
                    case '\'':
                        out.push_back(escape);
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u':
                    {
                        std::uint32_t code = 0;
                        if (!hex4(code))
                            return false;
                        if (code >= 0xD800 && code <= 0xDBFF && position_ + 6 <= text_.size() &&
                            text_[position_] == '\\' && text_[position_ + 1] == 'u')
                        {
                            const std::size_t saved = position_;
                            position_ += 2;
                            std::uint32_t low = 0;
                            if (!hex4(low))
                                return false;
                            if (low >= 0xDC00 && low <= 0xDFFF)
                                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            else
                                position_ = saved;
                        }
                        if (code >= 0xD800 && code <= 0xDFFF)
                            code = 0xFFFD;
                        append_utf8(out, code);
                        break;
                    }
                    default:
                        return fail("Invalid escape sequence");
                    }
                }
                return fail("Unterminated string");
            }

            bool parse_number(value& out)
            {
                const std::size_t start = position_;
                bool fraction = false;
                if (text_[position_] == '-')
                    ++position_;
                while (position_ < text_.size())
                {
                    const char c = text_[position_];
                    if (c >= '0' && c <= '9')
                    {
                        ++position_;
                    }
                    else if (c == '.' || c == 'e' || c == 'E' || c == '+' || (c == '-' && position_ > start))
                    {
                        fraction = true;
                        ++position_;
                    }
                    else
                    {
                        break;
                    }
                }
                const std::string_view literal_text = text_.substr(start, position_ - start);
                double parsed = 0;
                const auto result = std::from_chars(literal_text.data(), literal_text.data() + literal_text.size(), parsed);
                if (result.ec != std::errc{} || result.ptr != literal_text.data() + literal_text.size())
                {
                    position_ = start;
                    return fail("Invalid number");
                }
                out.kind = type::number;
                out.number = parsed;
                if (!fraction)
                {
                    long long whole = 0;
                    const auto integral = std::from_chars(literal_text.data(), literal_text.data() + literal_text.size(), whole);
                    if (integral.ec == std::errc{} && integral.ptr == literal_text.data() + literal_text.size() &&
                        whole >= std::numeric_limits<int>::min() && whole <= std::numeric_limits<int>::max())
                    {
                        out.integer = true;
                        out.integer_value = static_cast<int>(whole);
                    }
                }
                return true;
            }

            std::string_view text_;
            int depth_limit_;
            std::size_t position_ = 0;
            const char* reason_ = nullptr;
            std::size_t failed_at_ = 0;
        };

        std::string utf16_to_utf8(const std::string& bytes, std::size_t skip, bool big_endian)
        {
            std::wstring wide;
            wide.reserve((bytes.size() - skip) / 2);
            for (std::size_t i = skip; i + 1 < bytes.size(); i += 2)
            {
                const auto a = static_cast<std::uint8_t>(bytes[i]);
                const auto b = static_cast<std::uint8_t>(bytes[i + 1]);
                wide.push_back(static_cast<wchar_t>(big_endian ? (a << 8) | b : (b << 8) | a));
            }
            return util::to_utf8(wide);
        }
    }

    const value* value::find(std::string_view key) const
    {
        if (kind != type::object)
            return nullptr;
        for (const auto& item : members)
        {
            if (item.key == key)
                return &item.data;
        }
        return nullptr;
    }

    bool parse(std::string_view text, value& out, std::string& error, int depth_limit)
    {
        out = {};
        parser reader(text, depth_limit);
        return reader.run(out, error);
    }

    bool read_file(const std::filesystem::path& path, std::size_t maximum_bytes, std::string& text, std::string& error)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = util::win32_message(GetLastError());
            return false;
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size))
        {
            error = util::win32_message(GetLastError());
            CloseHandle(file);
            return false;
        }
        if (static_cast<unsigned long long>(size.QuadPart) > maximum_bytes)
        {
            CloseHandle(file);
            error = "A profile must be 2 MB or smaller.";
            return false;
        }
        std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
        DWORD read = 0;
        const bool ok = bytes.empty() ||
            (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size());
        const DWORD code = GetLastError();
        CloseHandle(file);
        if (!ok)
        {
            error = util::win32_message(code);
            return false;
        }

        if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0)
            text = bytes.substr(3);
        else if (bytes.size() >= 2 && bytes[0] == '\xFF' && bytes[1] == '\xFE')
            text = utf16_to_utf8(bytes, 2, false);
        else if (bytes.size() >= 2 && bytes[0] == '\xFE' && bytes[1] == '\xFF')
            text = utf16_to_utf8(bytes, 2, true);
        else
            text = std::move(bytes);
        return true;
    }

    std::string quote(std::string_view text)
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        out.reserve(text.size() + 2);
        out.push_back('"');
        for (const char c : text)
        {
            const auto byte = static_cast<unsigned char>(c);
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            default:
                if (byte < 0x20)
                {
                    out += "\\u00";
                    out.push_back(digits[byte >> 4]);
                    out.push_back(digits[byte & 15]);
                }
                else
                {
                    out.push_back(c);
                }
                break;
            }
        }
        out.push_back('"');
        return out;
    }
}
