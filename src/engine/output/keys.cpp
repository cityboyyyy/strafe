#include "keys.h"

namespace engine::output
{
    namespace
    {
        struct key_row
        {
            std::uint32_t code;
            std::string_view name;
            std::string_view display;
        };

        struct alias_row
        {
            std::uint32_t code;
            std::string_view name;
        };

        constexpr key_row key_rows[] =
        {
            { 0x08, "Back", "Back" },
            { 0x09, "Tab", "Tab" },
            { 0x0A, "LineFeed", "LineFeed" },
            { 0x0C, "Clear", "Clear" },
            { 0x0D, "Return", "Enter" },
            { 0x10, "ShiftKey", "ShiftKey" },
            { 0x11, "ControlKey", "ControlKey" },
            { 0x12, "Menu", "Menu" },
            { 0x13, "Pause", "Pause" },
            { 0x14, "Capital", "Capital" },
            { 0x15, "KanaMode", "KanaMode" },
            { 0x17, "JunjaMode", "JunjaMode" },
            { 0x18, "FinalMode", "FinalMode" },
            { 0x19, "HanjaMode", "HanjaMode" },
            { 0x1B, "Escape", "Escape" },
            { 0x1C, "IMEConvert", "IMEConvert" },
            { 0x1D, "IMENonconvert", "IMENonconvert" },
            { 0x1E, "IMEAceept", "IMEAceept" },
            { 0x1F, "IMEModeChange", "IMEModeChange" },
            { 0x20, "Space", "Space" },
            { 0x21, "PageUp", "PgUp" },
            { 0x22, "Next", "PgDn" },
            { 0x23, "End", "End" },
            { 0x24, "Home", "Home" },
            { 0x25, "Left", "Left" },
            { 0x26, "Up", "Up" },
            { 0x27, "Right", "Right" },
            { 0x28, "Down", "Down" },
            { 0x29, "Select", "Select" },
            { 0x2A, "Print", "Print" },
            { 0x2B, "Execute", "Execute" },
            { 0x2C, "PrintScreen", "PrintScreen" },
            { 0x2D, "Insert", "Ins" },
            { 0x2E, "Delete", "Del" },
            { 0x2F, "Help", "Help" },
            { 0x30, "D0", "0" },
            { 0x31, "D1", "1" },
            { 0x32, "D2", "2" },
            { 0x33, "D3", "3" },
            { 0x34, "D4", "4" },
            { 0x35, "D5", "5" },
            { 0x36, "D6", "6" },
            { 0x37, "D7", "7" },
            { 0x38, "D8", "8" },
            { 0x39, "D9", "9" },
            { 0x41, "A", "A" },
            { 0x42, "B", "B" },
            { 0x43, "C", "C" },
            { 0x44, "D", "D" },
            { 0x45, "E", "E" },
            { 0x46, "F", "F" },
            { 0x47, "G", "G" },
            { 0x48, "H", "H" },
            { 0x49, "I", "I" },
            { 0x4A, "J", "J" },
            { 0x4B, "K", "K" },
            { 0x4C, "L", "L" },
            { 0x4D, "M", "M" },
            { 0x4E, "N", "N" },
            { 0x4F, "O", "O" },
            { 0x50, "P", "P" },
            { 0x51, "Q", "Q" },
            { 0x52, "R", "R" },
            { 0x53, "S", "S" },
            { 0x54, "T", "T" },
            { 0x55, "U", "U" },
            { 0x56, "V", "V" },
            { 0x57, "W", "W" },
            { 0x58, "X", "X" },
            { 0x59, "Y", "Y" },
            { 0x5A, "Z", "Z" },
            { 0x5B, "LWin", "LWin" },
            { 0x5C, "RWin", "RWin" },
            { 0x5D, "Apps", "Apps" },
            { 0x5F, "Sleep", "Sleep" },
            { 0x60, "NumPad0", "NumPad0" },
            { 0x61, "NumPad1", "NumPad1" },
            { 0x62, "NumPad2", "NumPad2" },
            { 0x63, "NumPad3", "NumPad3" },
            { 0x64, "NumPad4", "NumPad4" },
            { 0x65, "NumPad5", "NumPad5" },
            { 0x66, "NumPad6", "NumPad6" },
            { 0x67, "NumPad7", "NumPad7" },
            { 0x68, "NumPad8", "NumPad8" },
            { 0x69, "NumPad9", "NumPad9" },
            { 0x6A, "Multiply", "Multiply" },
            { 0x6B, "Add", "Add" },
            { 0x6C, "Separator", "Separator" },
            { 0x6D, "Subtract", "Subtract" },
            { 0x6E, "Decimal", "Decimal" },
            { 0x6F, "Divide", "Divide" },
            { 0x70, "F1", "F1" },
            { 0x71, "F2", "F2" },
            { 0x72, "F3", "F3" },
            { 0x73, "F4", "F4" },
            { 0x74, "F5", "F5" },
            { 0x75, "F6", "F6" },
            { 0x76, "F7", "F7" },
            { 0x77, "F8", "F8" },
            { 0x78, "F9", "F9" },
            { 0x79, "F10", "F10" },
            { 0x7A, "F11", "F11" },
            { 0x7B, "F12", "F12" },
            { 0x7C, "F13", "F13" },
            { 0x7D, "F14", "F14" },
            { 0x7E, "F15", "F15" },
            { 0x7F, "F16", "F16" },
            { 0x80, "F17", "F17" },
            { 0x81, "F18", "F18" },
            { 0x82, "F19", "F19" },
            { 0x83, "F20", "F20" },
            { 0x84, "F21", "F21" },
            { 0x85, "F22", "F22" },
            { 0x86, "F23", "F23" },
            { 0x87, "F24", "F24" },
            { 0x90, "NumLock", "NumLock" },
            { 0x91, "Scroll", "Scroll" },
            { 0xA0, "LShiftKey", "LShiftKey" },
            { 0xA1, "RShiftKey", "RShiftKey" },
            { 0xA2, "LControlKey", "LControlKey" },
            { 0xA3, "RControlKey", "RControlKey" },
            { 0xA4, "LMenu", "LMenu" },
            { 0xA5, "RMenu", "RMenu" },
            { 0xA6, "BrowserBack", "BrowserBack" },
            { 0xA7, "BrowserForward", "BrowserForward" },
            { 0xA8, "BrowserRefresh", "BrowserRefresh" },
            { 0xA9, "BrowserStop", "BrowserStop" },
            { 0xAA, "BrowserSearch", "BrowserSearch" },
            { 0xAB, "BrowserFavorites", "BrowserFavorites" },
            { 0xAC, "BrowserHome", "BrowserHome" },
            { 0xAD, "VolumeMute", "VolumeMute" },
            { 0xAE, "VolumeDown", "VolumeDown" },
            { 0xAF, "VolumeUp", "VolumeUp" },
            { 0xB0, "MediaNextTrack", "MediaNextTrack" },
            { 0xB1, "MediaPreviousTrack", "MediaPreviousTrack" },
            { 0xB2, "MediaStop", "MediaStop" },
            { 0xB3, "MediaPlayPause", "MediaPlayPause" },
            { 0xB4, "LaunchMail", "LaunchMail" },
            { 0xB5, "SelectMedia", "SelectMedia" },
            { 0xB6, "LaunchApplication1", "LaunchApplication1" },
            { 0xB7, "LaunchApplication2", "LaunchApplication2" },
            { 0xBA, "Oem1", "Oem1" },
            { 0xBB, "Oemplus", "Oemplus" },
            { 0xBC, "Oemcomma", "Oemcomma" },
            { 0xBD, "OemMinus", "OemMinus" },
            { 0xBE, "OemPeriod", "OemPeriod" },
            { 0xBF, "OemQuestion", "OemQuestion" },
            { 0xC0, "Oemtilde", "Oemtilde" },
            { 0xDB, "OemOpenBrackets", "OemOpenBrackets" },
            { 0xDC, "Oem5", "Oem5" },
            { 0xDD, "Oem6", "Oem6" },
            { 0xDE, "Oem7", "Oem7" },
            { 0xDF, "Oem8", "Oem8" },
            { 0xE2, "OemBackslash", "OemBackslash" },
            { 0xE5, "ProcessKey", "ProcessKey" },
            { 0xE7, "Packet", "Packet" },
            { 0xF6, "Attn", "Attn" },
            { 0xF7, "Crsel", "Crsel" },
            { 0xF8, "Exsel", "Exsel" },
            { 0xF9, "EraseEof", "EraseEof" },
            { 0xFA, "Play", "Play" },
            { 0xFB, "Zoom", "Zoom" },
            { 0xFC, "NoName", "NoName" },
            { 0xFD, "Pa1", "Pa1" },
            { 0xFE, "OemClear", "OemClear" },
        };

        constexpr alias_row alias_rows[] =
        {
            { 0x0D, "Enter" },
            { 0x14, "CapsLock" },
            { 0x15, "HangulMode" },
            { 0x15, "HanguelMode" },
            { 0x19, "KanjiMode" },
            { 0x1E, "IMEAccept" },
            { 0x21, "Prior" },
            { 0x22, "PageDown" },
            { 0x2C, "Snapshot" },
            { 0xBA, "OemSemicolon" },
            { 0xBF, "Oem2" },
            { 0xC0, "Oem3" },
            { 0xDB, "Oem4" },
            { 0xDC, "OemPipe" },
            { 0xDD, "OemCloseBrackets" },
            { 0xDE, "OemQuotes" },
            { 0xE2, "Oem102" },
        };

        const key_row* row(std::uint32_t code)
        {
            for (const auto& item : key_rows)
            {
                if (item.code == code)
                    return &item;
            }
            return nullptr;
        }
    }

    bool key_defined(std::uint32_t code)
    {
        return row(code) != nullptr;
    }

    std::optional<std::string_view> key_name(std::uint32_t code)
    {
        if (const key_row* item = row(code))
            return item->name;
        return std::nullopt;
    }

    std::optional<std::string_view> key_display_name(std::uint32_t code)
    {
        if (const key_row* item = row(code))
            return item->display;
        return std::nullopt;
    }

    std::optional<std::uint32_t> key_from_name(std::string_view name)
    {
        for (const auto& item : key_rows)
        {
            if (item.name == name)
                return item.code;
        }
        for (const auto& item : alias_rows)
        {
            if (item.name == name)
                return item.code;
        }
        return std::nullopt;
    }
}
