#include <engine/engine.h>

#include "bindings/bindings.h"
#include "input/controller.h"
#include "input/playstation_input.h"
#include "output/output.h"
#include "profiles/json.h"
#include "profiles/profile.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool condition, const char* what)
    {
        if (!condition)
        {
            ++failures;
            std::printf("FAIL: %s\n", what);
        }
    }

    void write_file(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    void test_names()
    {
        using engine::output_action;
        check(engine::output_name(engine::key_output(0xA0)) == "Left Shift", "left shift name");
        check(engine::output_name(engine::key_output(0x0D)) == "Enter", "enter name");
        check(engine::output_name(engine::key_output(0x31)) == "1", "digit name");
        check(engine::output_name(engine::key_output(0x22)) == "PgDn", "page down name");
        check(engine::output_name(output_action::wheel_up) == "Wheel up", "wheel name");
        check(engine::output_name(engine::xbox_output(engine::control_button::lb)) == "Xbox Left bumper", "xbox name");
        check(engine::input_name(engine::controller_input(engine::control_button::a)) == "A / Cross", "a name");
        check(engine::input_name(engine::controller_input(engine::control_button::paddle_2)) == "Paddle P2", "paddle name");
        check(engine::xbox_output(engine::control_button::paddle_1) == output_action::none, "paddles are not xbox outputs");
        check(engine::xbox_outputs().size() == 25, "xbox outputs count");
    }

    void test_serialize()
    {
        using engine::output_action;
        check(engine::serialize_output(engine::key_output(0x22)) == "Key:Next", "serialize page down");
        check(engine::serialize_output(engine::key_output(0x0D)) == "Key:Return", "serialize enter");
        check(engine::serialize_output(output_action::mouse_back) == "MouseBack", "serialize mouse");
        check(engine::serialize_output(engine::xbox_output(engine::control_button::right_stick_right)) == "Xbox:RightStickRight",
            "serialize xbox");

        output_action parsed = output_action::none;
        check(engine::parse_output("Key:PageDown", parsed) && parsed == engine::key_output(0x22), "parse alias");
        check(engine::parse_output("Key:ShiftKey", parsed) && parsed == engine::key_output(0xA0), "parse normalizes shift");
        check(engine::parse_output("Key:65", parsed) && parsed == engine::key_output(0x41), "parse numeric key");
        check(!engine::parse_output("Key:Packet", parsed), "reject packet");
        check(!engine::parse_output("Key:LButton", parsed), "reject mouse key");
        check(!engine::parse_output("key:A", parsed), "case sensitive prefix");
        check(engine::parse_output("Xbox:Guide", parsed) && parsed == engine::xbox_output(engine::control_button::guide), "parse guide");
        check(!engine::parse_output("Xbox:Touchpad", parsed), "reject touchpad xbox");
        check(!engine::parse_output("Xbox:Paddle1", parsed), "reject paddle xbox");
        check(engine::parse_output("MoveLeft", parsed) && parsed == output_action::move_left, "parse move");
        check(!engine::parse_output("Xbox", parsed), "reject bare xbox");
    }

    void test_capture()
    {
        check(engine::capture_key(0x10, static_cast<std::int64_t>(0x36) << 16) == 0xA1, "capture right shift");
        check(engine::capture_key(0x10, static_cast<std::int64_t>(0x2A) << 16) == 0xA0, "capture left shift");
        check(engine::capture_key(0x11, 1ll << 24) == 0xA3, "capture right ctrl");
        check(engine::capture_key(0x12, 0) == 0xA4, "capture left alt");
    }

    void test_decode()
    {
        using engine::control_button;
        engine::input::gamepad pad;
        pad.left_y = 18000;
        pad.right_trigger = 36;
        auto pressed = engine::input::controller::decode(pad, control_button::none);
        check(any(pressed & control_button::left_stick_up) && any(pressed & control_button::rt), "press thresholds");
        pad.left_y = 15000;
        pad.right_trigger = 30;
        check(engine::input::controller::decode(pad, control_button::none) == control_button::none, "below press");
        check(engine::input::controller::decode(pad, pressed) == pressed, "hysteresis holds");
        pad.buttons = 0x0400 | 0x1000;
        check(engine::input::controller::decode(pad, control_button::none) == control_button::a, "guide masked from xinput");
    }

    void test_playstation()
    {
        using engine::control_button;
        using engine::input::playstation_input;
        using engine::input::playstation_model;
        std::uint8_t report[64]{};
        report[0] = 0x01;
        report[1] = 128;
        report[2] = 0;
        report[3] = 128;
        report[4] = 128;
        report[5] = 255;
        report[6] = 0;
        report[8] = 0x20 | 0x08;
        report[9] = 0x01;
        report[10] = 0x01 | 0x04 | 0x40 | 0x10;
        control_button pressed = control_button::none;
        engine::input::gamepad raw;
        check(playstation_input::try_decode(playstation_model::dual_sense_edge, false, report, sizeof(report), 0, sizeof(report),
            control_button::none, pressed, raw), "decode edge usb");
        const control_button expected = control_button::a | control_button::lb | control_button::left_stick_up |
            control_button::lt | control_button::guide | control_button::mute | control_button::left_paddle |
            control_button::left_function;
        check(pressed == expected, "edge usb buttons");
        check(raw.left_y == 32767, "edge left y inverted");

        check(playstation_input::try_decode(playstation_model::dual_sense, false, report, sizeof(report), 0, sizeof(report),
            control_button::none, pressed, raw), "decode dualsense usb");
        check(!any(pressed & control_button::left_paddle), "plain dualsense has no paddles");

        std::uint8_t simple[10]{};
        simple[0] = 0x01;
        simple[1] = simple[2] = simple[3] = simple[4] = 128;
        simple[5] = 0x10 | 0x0F;
        simple[6] = 0x20;
        simple[7] = 0x02 | 0x04;
        control_button simple_pressed = control_button::none;
        check(playstation_input::try_decode(playstation_model::dual_shock_4, false, simple, sizeof(simple), 0, sizeof(simple),
            control_button::none, simple_pressed, raw), "decode ds4 simple");
        check(simple_pressed == (control_button::x | control_button::start | control_button::touchpad), "ds4 simple buttons");
    }

    void test_profiles(const std::filesystem::path& folder)
    {
        using engine::control_button;
        engine::binding_map map = engine::default_bindings();
        check(map.size() == 17, "default count");
        map.set(engine::keyboard_input(0x41), engine::binding{ { engine::output_action::mouse_left, engine::key_output(0x10) }, true });
        map.set(engine::controller_input(control_button::paddle_1), engine::binding{ { engine::xbox_output(control_button::a) }, false });
        map.set(engine::mouse_input(engine::output_action::move_up), engine::binding{ { engine::xbox_output(control_button::right_stick_up) }, false });

        const auto path = folder / "round_trip.json";
        std::string error;
        check(engine::save_profile(path, map, error), "save profile");
        check(engine::save_profile(path, map, error), "save over existing profile");
        engine::binding_map loaded;
        check(engine::load_profile(path, loaded, error), "load profile");
        check(loaded.size() == map.size(), "round trip size");
        for (const auto& row : map)
        {
            const auto* other = loaded.find(row.source);
            check(other != nullptr && *other == row.value, "round trip row");
        }

        write_file(folder / "v1.json", "{\"version\":1,\"bindings\":{\"A\":\"Key:Space\",\"LT\":\"MouseRight\"}}");
        check(engine::load_profile(folder / "v1.json", loaded, error) && loaded.size() == 2, "load v1");

        write_file(folder / "dup.json", "{\"version\":2,\"bindings\":{\"Key:Return\":{\"targets\":[\"Key:A\"],\"suppress\":false},"
            "\"Key:Enter\":{\"targets\":[\"Key:B\"],\"suppress\":false}}}");
        check(!engine::load_profile(folder / "dup.json", loaded, error) && error == "Invalid or duplicate binding for 'Key:Enter'.",
            "duplicate rejected");

        write_file(folder / "bad.json", "{\"version\":2.0,\"bindings\":{}}");
        check(!engine::load_profile(folder / "bad.json", loaded, error) && error == "This file is not a supported Pad Bindings profile.",
            "float version rejected");

        write_file(folder / "array.json", "[1,2]");
        check(!engine::load_profile(folder / "array.json", loaded, error) && error == "The profile must contain a JSON object.",
            "array rejected");

        write_file(folder / "bom.json", "\xEF\xBB\xBF{\"version\":2,\"bindings\":{\"Controller:Paddle4\":{\"targets\":[\"WheelDown\"],\"suppress\":true}}}");
        check(engine::load_profile(folder / "bom.json", loaded, error) && loaded.size() == 1, "bom and paddle profile");
    }

    void test_rewasd(const std::filesystem::path& folder)
    {
        const std::string text = R"({
  "schemaVersion": 3.0,
  "devices": {
    "hardware": [ { "id": 1, "name": "gamepad" }, { "id": 2, "name": "keyboard" }, { "id": 3, "name": "mouse" } ],
    "virtual": { "gamepads": [ { "id": 5, "type": "360" } ] }
  },
  "shifts": [ { "id": 1, "type": "default" } ],
  "masks": [
    { "id": 1, "set": [ { "deviceId": 1, "buttonId": 1 } ] },
    { "id": 2, "set": [ { "deviceId": 2, "buttonId": 30 } ] },
    { "id": 3, "set": [ { "deviceId": 1, "buttonId": 51 } ] }
  ],
  "mappings": [
    { "condition": { "shiftId": 1, "mask": { "id": 1, "activator": { "type": "single", "mode": "hold_until_release" } } },
      "macros": [ { "keyboard": { "buttonId": 57 } } ] },
    { "condition": { "mask": { "id": 1, "activator": { "type": "single", "mode": "hold_until_release" } } },
      "macros": [ { "gamepad": { "id": 5, "buttonId": 2 } } ] },
    { "condition": { "mask": { "id": 2, "activator": { "type": "single", "mode": "hold_until_release" } } },
      "macros": [ { "mouse": { "buttonId": 1 } } ] },
    { "condition": { "mask": { "id": 3, "activator": { "type": "single", "mode": "hold_until_release" } } },
      "macros": [ { "gamepad": { "id": 5, "buttonId": 55, "value": 255 } } ] },
    { "condition": { "mask": { "id": 3, "activator": { "type": "double", "mode": "hold_until_release" } } },
      "macros": [ { "keyboard": { "buttonId": 57 } } ] },
    { "condition": { "shiftId": 1, "hardware": { "deviceId": 2, "buttonId": 30 } }, "hardware": { "unmap": true } }
  ]
})";
        write_file(folder / "test.rewasd", text);
        engine::import_result result;
        std::string error;
        check(engine::import_rewasd(folder / "test.rewasd", result, error), "import rewasd");
        check(result.skipped == 2, "rewasd skipped");
        check(result.bindings.size() == 3, "rewasd count");
        const auto* a = result.bindings.find(engine::controller_input(engine::control_button::a));
        check(a != nullptr && a->targets.size() == 1 && a->targets[0] == engine::key_output(0x20), "rewasd a to space");
        const auto* key = result.bindings.find(engine::keyboard_input(0x41));
        check(key != nullptr && key->suppress && key->targets[0] == engine::output_action::mouse_left, "rewasd suppressed key");
        const auto* lt = result.bindings.find(engine::controller_input(engine::control_button::lt));
        check(lt != nullptr && lt->targets[0] == engine::xbox_output(engine::control_button::rt), "rewasd xbox target");
    }

    void test_remapper()
    {
        engine::remapper remapper;
        const auto state = remapper.state();
        std::printf("desktop=%d motion=%d playstation=%d controller=%s paddles=%d error=%s\n", state.desktop_ready,
            state.motion_ready, state.playstation_ready, state.controller.name.c_str(), state.controller.paddles_available,
            state.error.c_str());
        check(state.desktop_ready, "desktop hooks ready");
        check(state.motion_ready, "raw mouse ready");
        check(state.devices.has_value(), "device counts");
        for (const auto& device : remapper.devices())
            std::printf("device kind=%d slot=%d selected=%d name=%s\n", static_cast<int>(device.kind), device.slot,
                device.selected, device.name.c_str());
        check(!remapper.start(), "start without bindings fails");
        check(remapper.state().error == "Add or import a binding before starting.", "empty start error");
        remapper.set_bindings(engine::default_bindings());
        check(remapper.start(), "start with defaults");
        check(remapper.state().active, "active after start");
        remapper.stop();
        check(!remapper.state().active, "inactive after stop");
    }
}

namespace
{
    std::string describe(const engine::binding_map& map)
    {
        std::vector<std::string> rows;
        for (const auto& row : map)
        {
            std::string line = std::to_string(static_cast<int>(row.source.kind)) + "|" + std::to_string(row.source.code) + "|" +
                (row.value.suppress ? "1" : "0");
            for (const auto target : row.value.targets)
                line += "|" + std::to_string(static_cast<std::uint32_t>(target));
            rows.push_back(line);
        }
        std::sort(rows.begin(), rows.end());
        std::string text;
        for (const auto& row : rows)
            text += row + "\n";
        return text;
    }

    int dump(const std::filesystem::path& folder)
    {
        std::string text;
        for (std::uint32_t code = 0; code < 0x100; ++code)
        {
            if (engine::is_key(code))
                text += "name|" + std::to_string(code) + "|" + engine::output_name(engine::key_output(code)) + "|" +
                    engine::serialize_output(engine::key_output(code)) + "\n";
        }
        for (const auto action : engine::mouse_outputs())
            text += "name|" + std::to_string(static_cast<std::uint32_t>(action)) + "|" + engine::output_name(action) + "|" +
                engine::serialize_output(action) + "\n";
        for (const auto action : engine::xbox_outputs())
            text += "name|" + std::to_string(static_cast<std::uint32_t>(action)) + "|" + engine::output_name(action) + "|" +
                engine::serialize_output(action) + "\n";
        for (const auto& item : engine::controller_inputs())
        {
            if (item.button <= engine::control_button::mute)
                text += "input|" + std::to_string(static_cast<std::uint64_t>(item.button)) + "|" +
                    engine::input_name(engine::controller_input(item.button)) + "\n";
        }
        std::ifstream cases(folder / "parse.txt");
        std::string line;
        while (std::getline(cases, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            engine::output_action action = engine::output_action::none;
            const bool ok = engine::parse_output(line, action);
            text += "parse|" + line + "|" + (ok ? std::to_string(static_cast<std::uint32_t>(action)) : "fail") + "\n";
        }
        engine::binding_map map;
        std::string error;
        if (engine::load_profile(folder / "profile.json", map, error))
        {
            text += "profile\n" + describe(map);
            engine::save_profile(folder / "saved_cpp.json", map, error);
        }
        else
        {
            text += "profile error|" + error + "\n";
        }
        engine::import_result imported;
        if (engine::import_rewasd(folder / "test.rewasd", imported, error))
            text += "rewasd|" + std::to_string(imported.skipped) + "\n" + describe(imported.bindings);
        else
            text += "rewasd error|" + error + "\n";
        write_file(folder / "dump_cpp.txt", text);
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc == 3 && std::string(argv[1]) == "--dump")
        return dump(argv[2]);
    const auto folder = std::filesystem::temp_directory_path() / "engine_tests";
    std::filesystem::create_directories(folder);
    test_names();
    test_serialize();
    test_capture();
    test_decode();
    test_playstation();
    test_profiles(folder);
    test_rewasd(folder);
    test_remapper();
    std::filesystem::remove_all(folder);
    std::printf(failures == 0 ? "all tests passed\n" : "%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
