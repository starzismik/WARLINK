#pragma once
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>

namespace discord
{
// Desktop RPC only: no OAuth token or bot credentials are needed.
class launcher_presence
{
    std::atomic<bool> running_{true};
    std::thread worker_;
    static bool send_frame(HANDLE pipe, unsigned opcode, const std::string& body)
    {
        const unsigned header[2]{opcode, static_cast<unsigned>(body.size())};
        std::string frame(reinterpret_cast<const char*>(header), sizeof(header));
        frame += body;
        DWORD written{};
        return WriteFile(pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr)
            && written == frame.size();
    }
    void run()
    {
        const auto started = std::time(nullptr);
        while (running_)
        {
            HANDLE pipe = INVALID_HANDLE_VALUE;
            for (int i = 0; i < 10 && running_; ++i)
            {
                const auto name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
                pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                if (pipe != INVALID_HANDLE_VALUE) break;
            }
            if (pipe != INVALID_HANDLE_VALUE)
            {
                bool connected = send_frame(pipe, 0, R"({"v":1,"client_id":"1566389290072809594"})");
                std::string pending;
                while (running_ && connected)
                {
                    DWORD available{};
                    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) break;
                    if (available)
                    {
                        char buffer[8192]; DWORD count{};
                        if (!ReadFile(pipe, buffer, (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &count, nullptr)) break;
                        pending.append(buffer, count);
                        while (pending.size() >= 8)
                        {
                            unsigned header[2]; std::memcpy(header, pending.data(), 8);
                            if (header[1] > 1024 * 1024) { connected = false; break; }
                            if (pending.size() < 8 + header[1]) break;
                            const auto body = pending.substr(8, header[1]);
                            if(body.find("warlink-launcher") != std::string::npos) { rapidjson::Document reply; reply.Parse(body.c_str()); std::ofstream log(std::filesystem::temp_directory_path()/"warlink-presence-status.txt"); log << (reply.HasMember("evt") && reply["evt"].IsString() ? reply["evt"].GetString() : "ACK") << "\n"; if(reply.HasMember("data") && reply["data"].IsObject() && reply["data"].HasMember("message") && reply["data"]["message"].IsString()) log << reply["data"]["message"].GetString(); }
                            pending.erase(0, 8 + header[1]);
                            if (header[0] == 2) { connected = false; break; }
                            if (header[0] == 3) connected = send_frame(pipe, 4, body);
                            if (header[0] == 1 && body.find("\"READY\"") != std::string::npos)
                            {
                                const auto activity = std::string(R"({"cmd":"SET_ACTIVITY","args":{"pid":)") + std::to_string(GetCurrentProcessId())
                                    + R"(,"activity":{"type":0,"details":"Launcher communautaire","state":"Dans WARLINK","timestamps":{"start":)" + std::to_string(started)
                                    + R"(},"assets":{"large_image":"https://raw.githubusercontent.com/starzismik/WARLINK/main/branding/warlink-profile-800.png","large_text":"WARLINK · par STARZISMIK"}}},"nonce":"warlink-launcher"})";
                                connected = send_frame(pipe, 1, activity);
                            }
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                CloseHandle(pipe); // Discord clears the activity when the launcher disconnects.
            }
            for (int i = 0; i < 30 && running_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
public:
    launcher_presence() : worker_([this] { run(); }) {}
    ~launcher_presence() { running_ = false; if (worker_.joinable()) worker_.join(); }
    launcher_presence(const launcher_presence&) = delete;
    launcher_presence& operator=(const launcher_presence&) = delete;
};
}
