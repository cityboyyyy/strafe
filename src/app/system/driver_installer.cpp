#include "driver_installer.h"

#include "../model/asset_store.h"

#include <cfgmgr32.h>
#include <newdev.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace menu::system::driver
{
    namespace
    {
        struct package_file
        {
            const wchar_t* asset;
            const wchar_t* name;
        };

        constexpr package_file duo_package[] = {
            { L"drivers/duocontroller/DuoController.dll", L"DuoController.dll" },
            { L"drivers/duocontroller/DuoController.inf", L"DuoController.inf" },
            { L"drivers/duocontroller/duocontroller.cat", L"DuoController.cat" },
        };
        constexpr const wchar_t* duo_marker = L"DuoController.installed";

        std::atomic<int> g_stage{ static_cast<int>(stage::idle) };
        std::mutex g_error_lock;
        std::string g_error;
        std::thread g_worker;

        void set(stage value)
        {
            g_stage.store(static_cast<int>(value));
        }

        void fail(std::string message)
        {
            {
                std::lock_guard lock(g_error_lock);
                g_error = std::move(message);
            }
            set(stage::failed);
        }

        bool same_contents(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
        {
            std::error_code error;
            if (std::filesystem::file_size(path, error) != bytes.size() || error)
                return false;
            std::ifstream file(path, std::ios::binary);
            std::vector<char> data(bytes.size());
            file.read(data.data(), static_cast<std::streamsize>(data.size()));
            return file && std::equal(data.begin(), data.end(), bytes.begin(), [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; });
        }

        bool write_asset(const wchar_t* asset, const std::filesystem::path& target)
        {
            const auto bytes = model::asset_bytes(asset);
            if (bytes.empty())
            {
                fail("The driver package is missing from this build.");
                return false;
            }
            if (same_contents(target, bytes))
                return true;
            std::error_code error;
            std::filesystem::create_directories(target.parent_path(), error);
            std::filesystem::remove(target, error);
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!out)
            {
                fail("Could not write the driver files. Close other copies of the app and retry.");
                return false;
            }
            return true;
        }

        bool duo_files_ready()
        {
            const std::filesystem::path directory = folder();
            for (const auto& file : duo_package)
            {
                const auto bytes = model::asset_bytes(file.asset);
                if (bytes.empty() || !same_contents(directory / file.name, bytes))
                    return false;
            }
            return true;
        }

        void run_duo()
        {
            const std::filesystem::path directory = folder();
            std::error_code error;
            std::filesystem::remove(directory / duo_marker, error);
            set(stage::extracting);
            for (const auto& file : duo_package)
            {
                if (!write_asset(file.asset, directory / file.name))
                    return;
            }
            set(stage::installing);
            const std::filesystem::path inf = directory / L"DuoController.inf";
            BOOL reboot = FALSE;
            if (!DiInstallDriverW(nullptr, inf.c_str(), 0, &reboot) && GetLastError() != ERROR_NO_MORE_ITEMS)
            {
                char message[96];
                std::snprintf(message, sizeof(message), "Windows rejected the driver package (0x%08lX).", GetLastError());
                fail(message);
                return;
            }
            std::ofstream(directory / duo_marker).put('1');
            set(stage::done);
        }

    }

    bool installed()
    {
        return duo_files_ready() && std::filesystem::exists(folder() / duo_marker);
    }

    bool elevated()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        TOKEN_ELEVATION elevation{};
        DWORD size = 0;
        const bool result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
        CloseHandle(token);
        return result;
    }

    bool busy()
    {
        const auto value = static_cast<stage>(g_stage.load());
        return value == stage::extracting || value == stage::installing;
    }

    void install()
    {
        if (busy())
            return;
        if (g_worker.joinable())
            g_worker.join();
        if (!elevated())
        {
            fail("Restart the app as administrator to install the driver.");
            return;
        }
        set(stage::extracting);
        g_worker = std::thread(run_duo);
    }

    status state()
    {
        status result;
        result.current = static_cast<stage>(g_stage.load());
        std::lock_guard lock(g_error_lock);
        result.error = g_error;
        return result;
    }

    void reset()
    {
        if (!busy())
            set(stage::idle);
    }

    void shutdown()
    {
        if (g_worker.joinable())
            g_worker.join();
    }

    std::filesystem::path folder()
    {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(L"ProgramFiles", buffer, MAX_PATH);
        const std::filesystem::path root = length > 0 && length < MAX_PATH ? std::filesystem::path(buffer) : std::filesystem::path(L"C:\\Program Files");
        return root / L"Strafe" / L"driver";
    }

    std::wstring library()
    {
        return installed() ? (folder() / L"DuoController.dll").wstring() : std::wstring();
    }
}
