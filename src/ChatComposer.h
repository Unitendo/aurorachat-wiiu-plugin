#pragma once

#include <atomic>
#include <functional>
#include <string>

namespace AuroraChat {

class ChatComposer {
public:
    using SubmitCallback = std::function<void(const std::string &text)>;

    static ChatComposer &Instance();

    bool Init();
    void Shutdown();

    bool Open(SubmitCallback onSubmit);
    bool IsActive() const { return mActive; }

    const std::string &GetText() const { return mBuffer; }

    int SelectedRow() const { return mRow; }
    int SelectedCol() const { return mCol; }

    void RunFrame();

    void RequestClose() { mForceClose = true; }

    void ForceCloseNow() { mActive = false; }

    bool IsReadingInput() const { return mReadingInput; }

private:
    ChatComposer() = default;
    ~ChatComposer() = default;
    ChatComposer(const ChatComposer &) = delete;
    ChatComposer &operator=(const ChatComposer &) = delete;

    void Close(bool submitted);
    void PressSelected();

    int RowLen(int row) const;

    bool mInitialized = false;
    std::atomic<bool> mActive{false};
    std::atomic<bool> mForceClose{false};
    bool mReadingInput = false;

    std::string mBuffer;
    SubmitCallback mOnSubmit;
    int mRow = 0;
    int mCol = 0;
};

} // namespace AuroraChat
