#include "client.h"
#include "URLCodec.h"
#include "utils/logger.h"

#include <notifications/notifications.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace AuroraChat {

    namespace {
        constexpr int kRecvTimeoutSec   = 2;
        constexpr int kReconnectDelayUs = 3 * 1000 * 1000;
        constexpr size_t kMaxLineLength = 64 * 1024;
    } // namespace

    Client &Client::Instance() {
        static Client instance;
        return instance;
    }

    void Client::Start(const Config &config) {
        if (mRunning.exchange(true)) {
            // Already running
            return;
        }

        mConfig = config;
        mThread = std::thread(&Client::ThreadMain, this);
    }

    void Client::Stop() {
        if (!mRunning.exchange(false)) {
            return;
        }

        if (mSocket >= 0) {
            shutdown(mSocket, SHUT_RDWR);
        }

        if (mThread.joinable()) {
            mThread.join();
        }

        Disconnect();
        mState = State::Disconnected;
    }

    bool Client::SendMessage(const std::string &message) {
        if (mState.load() != State::Ready) {
            return false;
        }
        return SendRaw("msg", {message});
    }

    bool Client::JoinRoom(const std::string &room) {
        if (mState.load() != State::WaitLoginOk && mState.load() != State::Ready) {
            return false;
        }
        mCurrentRoom = room;
        mState       = State::WaitJoinOk;
        return SendRaw("join", {room});
    }

    void Client::ThreadMain() {
        initLogging();
        DEBUG_FUNCTION_LINE("AuroraChat client thread started");

        int failCount = 0;
        while (mRunning.load()) {
            if (!Connect()) {
                DEBUG_FUNCTION_LINE("AuroraChat connect failed, retrying");

                if (failCount == 0 || failCount % 5 == 0) {
                    ShowError("AuroraChat: could not reach " + mConfig.host + ":" + std::to_string(mConfig.port));
                }
                failCount++;

                for (int i = 0; i < 10 && mRunning.load(); i++) {
                    usleep(kReconnectDelayUs / 10);
                }
                continue;
            }
            failCount = 0;

            mState = State::WaitHello;
            mRecvBuffer.clear();

            char buf[2048];
            while (mRunning.load()) {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(mSocket, &readSet);

                struct timeval tv {};
                tv.tv_sec  = kRecvTimeoutSec;
                tv.tv_usec = 0;

                int selected = select(mSocket + 1, &readSet, nullptr, nullptr, &tv);
                if (selected < 0) {
                    DEBUG_FUNCTION_LINE("AuroraChat select() error: %d", errno);
                    break;
                }
                if (selected == 0) {
                    continue; // timed out
                }

                ssize_t received = recv(mSocket, buf, sizeof(buf), 0);
                if (received > 0) {
                    mRecvBuffer.append(buf, static_cast<size_t>(received));

                    size_t pos;
                    while ((pos = mRecvBuffer.find('\n')) != std::string::npos) {
                        std::string line = mRecvBuffer.substr(0, pos);
                        mRecvBuffer.erase(0, pos + 1);
                        if (!line.empty() && line.back() == '\r') {
                            line.pop_back();
                        }
                        if (!line.empty()) {
                            ProcessLine(line);
                        }
                    }

                    if (mRecvBuffer.size() > kMaxLineLength) {
                        DEBUG_FUNCTION_LINE("AuroraChat line too long, disconnecting");
                        break;
                    }
                } else if (received == 0) {
                    DEBUG_FUNCTION_LINE("AuroraChat server closed the connection");
                    break;
                } else {
                    DEBUG_FUNCTION_LINE("AuroraChat recv error: %d", errno);
                    break;
                }
            }

            Disconnect();
            mState = State::Disconnected;

            if (mRunning.load()) {
                ShowError("Disconnected from AuroraChat, reconnecting...");
                for (int i = 0; i < 10 && mRunning.load(); i++) {
                    usleep(kReconnectDelayUs / 10);
                }
            }
        }

        DEBUG_FUNCTION_LINE("AuroraChat client thread exiting");
        deinitLogging();
    }

    bool Client::Connect() {
        struct addrinfo hints {};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *result = nullptr;
        char portStr[8];
        snprintf(portStr, sizeof(portStr), "%u", mConfig.port);

        if (getaddrinfo(mConfig.host.c_str(), portStr, &hints, &result) != 0 || result == nullptr) {
            return false;
        }

        mSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (mSocket < 0) {
            freeaddrinfo(result);
            return false;
        }

        bool connected = connect(mSocket, result->ai_addr, result->ai_addrlen) == 0;
        freeaddrinfo(result);

        if (!connected) {
            DEBUG_FUNCTION_LINE("AuroraChat connect() failed, errno=%d", errno);
            close(mSocket);
            mSocket = -1;
            return false;
        }

        DEBUG_FUNCTION_LINE("AuroraChat connected to %s:%u", mConfig.host.c_str(), mConfig.port);
        return true;
    }


    void Client::Disconnect() {
        if (mSocket >= 0) {
            close(mSocket);
            mSocket = -1;
        }
    }

    bool Client::SendRaw(const std::string &command, const std::vector<std::string> &args) {
        if (mSocket < 0) {
            return false;
        }

        std::string out = command;
        out.push_back('|');
        for (const auto &arg : args) {
            out += URLCodec::Encode(arg);
            out.push_back('|');
        }
        out.push_back('\n');

        size_t sent = 0;
        while (sent < out.size()) {
            ssize_t n = send(mSocket, out.data() + sent, out.size() - sent, 0);
            if (n <= 0) {
                return false;
            }
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    std::vector<std::string> Client::SplitArgs(const std::string &body) {
        std::vector<std::string> args;
        size_t start = 0;
        size_t pos;
        while ((pos = body.find('|', start)) != std::string::npos) {
            args.push_back(URLCodec::Decode(body.substr(start, pos - start)));
            start = pos + 1;
        }

        if (start < body.size()) {
            args.push_back(URLCodec::Decode(body.substr(start)));
        }
        return args;
    }

    void Client::ProcessLine(const std::string &line) {
        size_t sep          = line.find('|');
        std::string command = sep == std::string::npos ? line : line.substr(0, sep);
        std::string body    = sep == std::string::npos ? std::string() : line.substr(sep + 1);

        for (auto &c : command) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        std::vector<std::string> args = SplitArgs(body);

        if (command == "hello") {
            std::string server = args.size() > 1 ? args[1] : "server";
            ShowInfo("Connected to " + server);

            mState = State::WaitLoginOk;
            SendRaw("login", {mConfig.login, mConfig.password});
        } else if (command == "ipbanned") {
            ShowError("You are IP-banned from this AuroraChat server");
            mRunning = false;
        } else if (command == "ok") {
            HandleOk();
        } else if (command == "err") {
            HandleErr(args);
        } else if (command == "rules") {
            if (!args.empty()) {
                ShowInfo("Rules: " + args[0]);
            }
        } else if (command == "motd") {
            if (!args.empty()) {
                ShowInfo("MOTD: " + args[0]);
            }
        } else if (command == "msg") {
            if (args.size() >= 2) {
                ShowInfo(args[0] + ": " + args[1]);
            }
        } else {
            DEBUG_FUNCTION_LINE("AuroraChat unhandled command: %s", command.c_str());
        }
    }

    void Client::HandleOk() {
        switch (mState.load()) {
            case State::WaitLoginOk:
                mCurrentRoom = mConfig.room;

                if (SendRaw("join", {mConfig.room})) {
                    mState = State::Ready;

                    ShowInfo("Joined " + mCurrentRoom);
                } else {
                    mState = State::Disconnected;
                }
                break;
            case State::WaitJoinOk:
                mState = State::Ready;
                ShowInfo("Joined " + mCurrentRoom);
                break;
            default:
                break;
        }
    }

    void Client::HandleErr(const std::vector<std::string> &args) {
        std::string code   = args.empty() ? "unknown" : args[0];
        std::string reason = args.size() > 1 ? (" (" + args[1] + ")") : "";
        ShowError("AuroraChat error: " + code + reason);

        if (code == "bad_login" && mState.load() == State::WaitLoginOk) {
            mRunning = false;
        }
    }

    void Client::ShowInfo(const std::string &text) const {
        NotificationModule_AddInfoNotification(text.c_str());
    }

    void Client::ShowError(const std::string &text) const {
        NotificationModule_AddErrorNotification(text.c_str());
    }

} // namespace AuroraChat
