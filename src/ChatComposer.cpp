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
        constexpr const char *kActionLabels[] = {"Shift", "Space", "Back", "Send"};

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
        mRow = 0;
        mCol = 0;

        mShifted  = false;
        mCapsLock = false;

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

    void ChatComposer::toggleCapsShift() {
        if (mCapsLock) {
            mCapsLock = false;
            mShifted  = false;
        } else if (mShifted) {
            mShifted  = false;
            mCapsLock = true;
        } else {
            mShifted = true;
        }
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
                char c = row[mCol];

                if (mCapsLock || mShifted) {
                    if (c >= 'a' && c <= 'z') {
                        c = c - 'a' + 'A';
                    }
                }

                mBuffer.push_back(c);

                if (mShifted && !mCapsLock) {
                    mShifted = false;
                }
            }
        } else {
            switch (mCol) {
                case 0: // Shift
                    toggleCapsShift();
                    break;
                case 1: // Space
                    mBuffer.push_back(' ');
                    break;
                case 2: // Back
                    if (!mBuffer.empty()) {
                        mBuffer.pop_back();
                    } else {
                        Close(false);
                    }
                    break;
                case 3: // Send
                    Close(true);
                    break;
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

        if (t & VPAD_BUTTON_B) { // Back
            if (!mBuffer.empty()) {
                mBuffer.pop_back();
            } else {
                Close(false);
            }
        } else if (t & VPAD_BUTTON_A) { // Action
            PressSelected();
        } else if (t & VPAD_BUTTON_X) { // Cancel
            Close(false);
        } else if (t & VPAD_BUTTON_Y) { // Space
            mBuffer.push_back(' ');
        } else if (t & VPAD_BUTTON_ZL || t & VPAD_BUTTON_ZR) { // Shift
            toggleCapsShift();
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
