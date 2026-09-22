#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace AuroraChat {

    // placeholder values
    struct Config {
        std::string host;
        uint16_t port = 0;
        std::string login;
        std::string password;
        std::string room;
        uint32_t historySize = 0;
    };


    class Client {
    public:
        static Client &Instance();

        void Start(const Config &config);

        void Stop();

        bool SendMessage(const std::string &message);

        bool JoinRoom(const std::string &room);

        bool IsReady() const { return mState.load() == State::Ready; }

    private:
        Client()               = default;
        ~Client()              = default;
        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;

        enum class State {
            Disconnected,
            WaitHello,
            WaitLoginOk,
            WaitJoinOk,
            Ready,
        };

        void ThreadMain();
        bool Connect();
        void Disconnect();

        bool SendRaw(const std::string &command, const std::vector<std::string> &args);
        void ProcessLine(const std::string &line);
        void HandleOk();
        void HandleErr(const std::vector<std::string> &args);

        static std::vector<std::string> SplitArgs(const std::string &body);

        void ShowInfo(const std::string &text) const;
        void ShowError(const std::string &text) const;

        Config mConfig{};
        int mSocket = -1;
        std::atomic<bool> mRunning{false};
        std::atomic<State> mState{State::Disconnected};
        std::thread mThread;
        std::string mRecvBuffer;
        std::string mCurrentRoom;
    };

} // namespace AuroraChat
