#include "ChatComposer.h"
#include "utils/logger.h"

#include <coreinit/cache.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <malloc.h>
#include <vpad/input.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace AuroraChat {


    namespace {

        constexpr const char *kLetterRows[] = {
                "1234567890",
                "qwertyuiop",
                "asdfghjkl",
                "zxcvbnm",
        };
        constexpr int kNumLetterRows          = 4;
        constexpr int kActionRow              = 4;
        constexpr int kNumActions             = 4;
        constexpr const char *kActionLabels[] = {"Space", "Back", "Cancel", "Send"};

    } // namespace


    ChatComposer &ChatComposer::Instance() {
        static ChatComposer instance;
        return instance;
    }

    bool ChatComposer::Init() {
        mInitialized = true;
        return true;
    }

    void ChatComposer::Shutdown() {
        mActive      = false;
        mInitialized = false;
    }

    bool ChatComposer::Open(SubmitCallback onSubmit) {
        if (!mInitialized || mActive) {
            return false;
        }
        mBuffer.clear();
        mRow      = 0;
        mCol      = 0;
        mOnSubmit = std::move(onSubmit);
        mActive   = true;
        return true;
    }

    void ChatComposer::Close(bool submitted) {
        if (submitted && mOnSubmit && !mBuffer.empty()) {
            mOnSubmit(mBuffer);
        }
        mActive = false;
    }

    int ChatComposer::RowLen(int row) const {
        if (row < kNumLetterRows) {
            return static_cast<int>(strlen(kLetterRows[row]));
        }
        return kNumActions;
    }

    void ChatComposer::PressSelected() {
        if (mRow < kNumLetterRows) {
            const char *row = kLetterRows[mRow];
            if (mCol < static_cast<int>(strlen(row))) {
                mBuffer.push_back(row[mCol]);
            }
        } else {
            switch (mCol) {
                case 0:
                    mBuffer.push_back(' ');
                    break; // Space
                case 1:
                    if (!mBuffer.empty()) mBuffer.pop_back();
                    break; // Backspace
                case 2:
                    Close(false);
                    break; // Cancel
                case 3:
                    Close(true);
                    break; // Send
            }
        }
    }

    void ChatComposer::RunFrame() {
        if (!mActive) {
            return;
        }

        if (mForceClose.exchange(false)) {
            Close(false);
            return;
        }

        VPADStatus vpad{};
        VPADReadError err;

        mReadingInput = true;

        const int32_t count = VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        mReadingInput = false;

        if (count <= 0 || err != VPAD_READ_SUCCESS) {
            return;
        }

        const uint32_t t = vpad.trigger;

        if (t & VPAD_BUTTON_B) {
            Close(false);
        } else if (t & VPAD_BUTTON_A) {
            PressSelected();
        } else if (t & VPAD_BUTTON_LEFT) {
            mCol = (mCol - 1 + RowLen(mRow)) % RowLen(mRow);
        } else if (t & VPAD_BUTTON_RIGHT) {
            mCol = (mCol + 1) % RowLen(mRow);
        } else if (t & VPAD_BUTTON_UP) {
            mRow = (mRow - 1 + kActionRow + 1) % (kActionRow + 1);
            mCol = std::min(mCol, RowLen(mRow) - 1);
        } else if (t & VPAD_BUTTON_DOWN) {
            mRow = (mRow + 1) % (kActionRow + 1);
            mCol = std::min(mCol, RowLen(mRow) - 1);
        }
    }

} // namespace AuroraChat
