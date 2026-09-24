#include "function_patches.h"
#include "ChatComposer.h"
#include "ColorShader.h"
#include "libs/SchriftGX2.h"
#include "shaders/Texture2DShader.h"
#include "utils/logger.h"

#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/state.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <gx2r/surface.h>
#include <memory/mappedmemory.h>
#include <wups.h>

#include <cstring>
#include <malloc.h>
#include <new>


namespace {

    GX2ContextState *sContextState         = nullptr;
    GX2ContextState *sOriginalContextState = nullptr;
    bool sContextStateReady                = false;
    SchriftGX2 *sFont                      = nullptr;

} // namespace


DECL_FUNCTION(void, GX2SetContextState_hook, GX2ContextState *curContext) {
    real_GX2SetContextState_hook(curContext);
    sOriginalContextState = curContext;
}

DECL_FUNCTION(void, GX2SetupContextStateEx_hook, GX2ContextState *state, BOOL unk1) {
    real_GX2SetupContextStateEx_hook(state, unk1);
    sOriginalContextState = state;
}

void Renderer_AllocateContextState() {
    sContextState = static_cast<GX2ContextState *>(MEMAllocFromMappedMemoryForGX2Ex(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));

    if (!sContextState) {
        DEBUG_FUNCTION_LINE("Renderer: failed to allocate GX2ContextState");
    } else {
        DEBUG_FUNCTION_LINE("Renderer: GX2ContextState allocated at %p", sContextState);
    }
}

void Renderer_Deinitialize() {
    delete sFont;
    sFont = nullptr;

    if (sContextState) {
        MEMFreeToMappedMemory(sContextState);
        sContextState = nullptr;
    }

    sContextStateReady    = false;
    sOriginalContextState = nullptr;
}


namespace {

    constexpr const char *kLetterRows[] = {
            "1234567890",
            "qwertyuiop",
            "asdfghjkl",
            "zxcvbnm",
    };

    constexpr int kNumLetterRows = 4;
    constexpr int kActionRow     = 4; // "row" index for the action buttons
    constexpr int kNumActions    = 4; // Shift, Space, Back, Send

    constexpr float kCanvasWidth  = 1280.0f;
    constexpr float kCanvasHeight = 720.0f;

    constexpr float kKeySize            = 64.0f;
    constexpr float kKeyGap             = 10.0f;
    constexpr float kGridStartX         = 140.0f;
    constexpr float kGridStartY         = 160.0f;
    constexpr float kActionButtonWidth  = 200.0f;
    constexpr float kActionButtonHeight = 64.0f;

    void RectToOffsetScale(float x, float y, float w, float h, float targetWidth, float targetHeight, float *outOffset, float *outScale) {
        const float scaleX = targetWidth / kCanvasWidth;
        const float scaleY = targetHeight / kCanvasHeight;

        const float actualX = x * scaleX;
        const float actualY = y * scaleY;
        const float actualW = w * scaleX;
        const float actualH = h * scaleY;

        float centerX = actualX + actualW / 2.0f - targetWidth / 2.0f;

        float centerY = actualY + actualH / 2.0f - targetHeight / 2.0f;

        outOffset[0] = centerX / targetWidth * 2.0f;

        outOffset[1] = -(centerY / targetHeight * 2.0f);

        outOffset[2] = 0.0f;
        outOffset[3] = 0.0f;

        outScale[0] = actualW / targetWidth;

        outScale[1] = actualH / targetHeight;

        outScale[2] = 1.0f;
        outScale[3] = 0.0f;
    }


    uint8_t *sNormalColor    = nullptr;
    uint8_t *sHighlightColor = nullptr;
    uint8_t *sShiftColor     = nullptr;
    uint8_t *sCapsLockColor  = nullptr;

    bool EnsureColorBuffers() {
        if (sNormalColor && sHighlightColor) {
            return true;
        }

        sNormalColor    = static_cast<uint8_t *>(MEMAllocFromMappedMemoryForGX2Ex(ColorShader::cuColorVtxsSize, GX2_VERTEX_BUFFER_ALIGNMENT));
        sHighlightColor = static_cast<uint8_t *>(MEMAllocFromMappedMemoryForGX2Ex(ColorShader::cuColorVtxsSize, GX2_VERTEX_BUFFER_ALIGNMENT));
        sShiftColor     = static_cast<uint8_t *>(MEMAllocFromMappedMemoryForGX2Ex(ColorShader::cuColorVtxsSize, GX2_VERTEX_BUFFER_ALIGNMENT));
        sCapsLockColor  = static_cast<uint8_t *>(MEMAllocFromMappedMemoryForGX2Ex(ColorShader::cuColorVtxsSize, GX2_VERTEX_BUFFER_ALIGNMENT));

        if (!sNormalColor || !sHighlightColor || !sShiftColor || !sCapsLockColor) {
            DEBUG_FUNCTION_LINE("Renderer: failed to allocate key color buffers");
            return false;
        }

        const uint8_t normal[]        = {60, 60, 60, 220, 60, 60, 60, 220, 60, 60, 60, 220, 60, 60, 60, 220};
        const uint8_t highlight[]     = {255, 220, 0, 255, 255, 220, 0, 255, 255, 220, 0, 255, 255, 220, 0, 255};
        const uint8_t shiftColor[]    = {100, 190, 255, 255, 100, 190, 255, 255, 100, 190, 255, 255, 100, 190, 255, 255};
        const uint8_t capsLockColor[] = {80, 200, 100, 255, 80, 200, 100, 255, 80, 200, 100, 255, 80, 200, 100, 255};
        memcpy(sNormalColor, normal, sizeof(normal));
        memcpy(sHighlightColor, highlight, sizeof(highlight));
        memcpy(sShiftColor, shiftColor, sizeof(shiftColor));
        memcpy(sCapsLockColor, capsLockColor, sizeof(capsLockColor));
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sNormalColor, ColorShader::cuColorVtxsSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sHighlightColor, ColorShader::cuColorVtxsSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sShiftColor, ColorShader::cuColorVtxsSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sCapsLockColor, ColorShader::cuColorVtxsSize);

        return true;
    }

    void DrawKeyLabel(const char *label, float x, float y, float w, float h, const float *color) {
        if (!sFont || !label) {
            return;
        }

        wchar_t wideLabel[8] = {};
        mbstowcs(wideLabel, label, sizeof(wideLabel) / sizeof(wideLabel[0]) - 1);

        constexpr int16_t fontSize = 56;

        const uint16_t textWidth = sFont->getWidth(wideLabel, fontSize);

        const float screenTextX         = x + (w - static_cast<float>(textWidth)) * 0.5f;
        constexpr float kKeyTextYOffset = 48.0f;
        const float screenTextY         = y + (h - static_cast<float>(fontSize)) * 0.5f + kKeyTextYOffset;

        const float textX = screenTextX - kCanvasWidth * 0.5f;
        const float textY = kCanvasHeight * 0.5f - screenTextY;

        const float blurColor[4] = {
                0.0f,
                0.0f,
                0.0f,
                1.0f};

        sFont->drawText(static_cast<int16_t>(textX), static_cast<int16_t>(textY), 0, wideLabel, fontSize, color, 0, 0, 0.0f, 0.0f, blurColor);
    }


    constexpr float kPreviewX      = 140.0f;
    constexpr float kPreviewY      = 60.0f;
    constexpr float kPreviewWidth  = 1000.0f;
    constexpr float kPreviewHeight = 70.0f;

    void DrawPreviewBox(ColorShader *shader, float targetWidth, float targetHeight) {
        float offset[4];
        float scale[4];

        RectToOffsetScale(kPreviewX, kPreviewY, kPreviewWidth, kPreviewHeight, targetWidth, targetHeight, offset, scale);

        shader->setOffset(offset);
        shader->setScale(scale);

        shader->setAttributeBuffer(sNormalColor);

        GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
    }

    void DrawPreviewText(const std::string &text) {
        if (!sFont || text.empty()) {
            return;
        }

        wchar_t wideText[256] = {};
        const size_t maxChars = sizeof(wideText) / sizeof(wideText[0]) - 1;

        size_t length = text.size();
        if (length > maxChars) {
            length = maxChars;
        }

        for (size_t i = 0; i < length; i++) {
            wideText[i] = static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
        }

        constexpr int16_t fontSize = 56;

        constexpr float kPreviewTextXOffset = 20.0f;
        constexpr float kPreviewTextYOffset = 16.0f;

        const float screenTextX = kPreviewX + kPreviewTextXOffset;
        const float screenTextY = kPreviewY + kPreviewHeight * 0.5f + kPreviewTextYOffset;

        const float textX = screenTextX - kCanvasWidth * 0.5f;
        const float textY = kCanvasHeight * 0.5f - screenTextY;

        const float textColor[4] = {
                1.0f,
                1.0f,
                1.0f,
                1.0f};

        const float blurColor[4] = {
                0.0f,
                0.0f,
                0.0f,
                1.0f};

        sFont->drawText(static_cast<int16_t>(textX), static_cast<int16_t>(textY), 0, wideText, fontSize, textColor, 0, 0, 0.0f, 0.0f, blurColor);
    }


    constexpr const char *kActionLabels[] = {
            "Shift",
            "Space",
            "Back",
            "Send",
    };

    void DrawKeyboardGrid(GX2ColorBuffer *colorBuffer) {
        if (!colorBuffer || !sContextStateReady) {
            return;
        }

        ColorShader *shader = ColorShader::instance();
        if (!shader || !EnsureColorBuffers()) {
            return;
        }

        AuroraChat::ChatComposer &composer = AuroraChat::ChatComposer::Instance();

        const int selectedRow = composer.SelectedRow();
        const int selectedCol = composer.SelectedCol();

        const bool shifted  = composer.IsShifted();
        const bool capsLock = composer.IsCapsLock();

        GX2ColorBuffer cb;
        GX2InitColorBuffer(&cb, colorBuffer->surface.dim, colorBuffer->surface.width, colorBuffer->surface.height, colorBuffer->surface.depth, colorBuffer->surface.format, colorBuffer->surface.aa, colorBuffer->surface.tileMode, colorBuffer->surface.swizzle, colorBuffer->aaBuffer, colorBuffer->aaSize);

        cb.surface.image = colorBuffer->surface.image;

        GX2ContextState *previousContext = sOriginalContextState;
        real_GX2SetContextState_hook(sContextState);
        GX2SetDefaultState();

        GX2SetColorBuffer(&cb, GX2_RENDER_TARGET_0);

        GX2SetViewport(0.0f, 0.0f, static_cast<float>(cb.surface.width), static_cast<float>(cb.surface.height), 0.0f, 1.0f);

        GX2SetScissor(0, 0, cb.surface.width, cb.surface.height);

        GX2SetDepthOnlyControl(GX2_FALSE, GX2_FALSE, GX2_COMPARE_FUNC_NEVER);

        GX2SetAlphaTest(GX2_FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);

        GX2SetColorControl(GX2_LOGIC_OP_COPY, GX2_ENABLE, GX2_DISABLE, GX2_ENABLE);


        shader->setAngle(0.0f);

        const float colorIntensity[4] = {
                1.0f,
                1.0f,
                1.0f,
                1.0f};

        shader->setColorIntensity(colorIntensity);
        shader->setShaders();

        const float targetWidth = static_cast<float>(cb.surface.width);

        const float targetHeight = static_cast<float>(cb.surface.height);

        auto drawKey = [&](float x, float y, float w, float h, bool highlighted, bool shifted, bool capsLock) {
            float offset[4];
            float scale[4];

            RectToOffsetScale(x, y, w, h, targetWidth, targetHeight, offset, scale);

            shader->setOffset(offset);
            shader->setScale(scale);

            if (capsLock) {
                shader->setAttributeBuffer(sCapsLockColor);
            } else if (shifted) {
                shader->setAttributeBuffer(sShiftColor);
            } else if (highlighted) {
                shader->setAttributeBuffer(sHighlightColor);
            } else {
                shader->setAttributeBuffer(sNormalColor);
            }

            GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
        };


        DrawPreviewBox(shader, targetWidth, targetHeight);

        // Draw all key rectangles

        for (int row = 0; row < kNumLetterRows; row++) {
            const char *chars = kLetterRows[row];

            for (int col = 0; chars[col]; col++) {
                const float x = kGridStartX + static_cast<float>(col) * (kKeySize + kKeyGap);
                const float y = kGridStartY + static_cast<float>(row) * (kKeySize + kKeyGap);

                const bool highlighted = row == selectedRow && col == selectedCol;

                drawKey(x, y, kKeySize, kKeySize, highlighted, false, false);
            }
        }

        for (int i = 0; i < kNumActions; i++) {
            const float x = kGridStartX + static_cast<float>(i) * (kActionButtonWidth + kKeyGap);
            const float y = kGridStartY + static_cast<float>(kNumLetterRows) * (kKeySize + kKeyGap);

            const bool highlighted = selectedRow == kActionRow && selectedCol == i;

            const bool shiftButton = i == 0;

            drawKey(x, y, kActionButtonWidth, kActionButtonHeight, highlighted, shiftButton && shifted, shiftButton && capsLock);
        }


        // Draw all text

        const std::string &text = composer.GetText();

        DrawPreviewText(text);

        const float textColor[4] = {
                1.0f,
                1.0f,
                1.0f,
                1.0f};

        for (int row = 0; row < kNumLetterRows; row++) {
            const char *chars = kLetterRows[row];

            for (int col = 0; chars[col]; col++) {
                const float x = kGridStartX + static_cast<float>(col) * (kKeySize + kKeyGap);
                const float y = kGridStartY + static_cast<float>(row) * (kKeySize + kKeyGap);

                char character = chars[col];

                if (shifted || capsLock) {
                    if (character >= 'a' && character <= 'z') {
                        character = character - 'a' + 'A';
                    }
                }

                char label[2] = {
                        character,
                        '\0'};

                DrawKeyLabel(label, x, y, kKeySize, kKeySize, textColor);
            }
        }

        for (int i = 0; i < kNumActions; i++) {
            const float x = kGridStartX + static_cast<float>(i) * (kActionButtonWidth + kKeyGap);
            const float y = kGridStartY + static_cast<float>(kNumLetterRows) * (kKeySize + kKeyGap);

            DrawKeyLabel(kActionLabels[i], x, y, kActionButtonWidth, kActionButtonHeight, textColor);
        }

        GX2Flush();

        if (previousContext) {
            real_GX2SetContextState_hook(previousContext);
        }
    }

} // namespace


DECL_FUNCTION(void, GX2Init_hook, uint32_t attributes) {
    real_GX2Init_hook(attributes);
    DEBUG_FUNCTION_LINE("Renderer: GX2Init hook fired");

    if (sContextState && !sContextStateReady) {
        real_GX2SetupContextStateEx_hook(sContextState, GX2_TRUE);
        DCInvalidateRange(sContextState, sizeof(GX2ContextState));
        sContextStateReady = true;
        DEBUG_FUNCTION_LINE("Renderer: private context initialized");
    }

    ColorShader::instance();
    DEBUG_FUNCTION_LINE("Renderer: ColorShader initialized");

    Texture2DShader::instance();

    void *font        = nullptr;
    uint32_t fontSize = 0;

    if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &font, &fontSize) && font && fontSize > 0) {
        sFont = new (std::nothrow) SchriftGX2(static_cast<uint8_t *>(font), static_cast<int32_t>(fontSize));
    }

    if (!sFont) {
        DEBUG_FUNCTION_LINE_ERR("AuroraChat: failed to initialize system font");
    }
}

DECL_FUNCTION(void, GX2CopyColorBufferToScanBuffer_hook, const GX2ColorBuffer *colorBuffer, GX2ScanTarget scanTarget) {
    if (colorBuffer && AuroraChat::ChatComposer::Instance().IsActive() && (scanTarget == GX2_SCAN_TARGET_TV || scanTarget == GX2_SCAN_TARGET_DRC)) {
        DrawKeyboardGrid(const_cast<GX2ColorBuffer *>(colorBuffer));
    }

    real_GX2CopyColorBufferToScanBuffer_hook(colorBuffer, scanTarget);
}

DECL_FUNCTION(void, GX2SwapScanBuffers_hook) {
    if (AuroraChat::ChatComposer::Instance().IsActive()) {
        AuroraChat::ChatComposer::Instance().RunFrame();
    }

    real_GX2SwapScanBuffers_hook();
}

DECL_FUNCTION(int32_t, VPADRead_hook, VPADChan chan, VPADStatus *buffers, uint32_t count, VPADReadError *error) {
    VPADReadError realError;

    const int32_t result = real_VPADRead_hook(chan, buffers, count, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        auto &composer = AuroraChat::ChatComposer::Instance();

        if (composer.IsActive()) {
            for (uint32_t i = 0; i < count; ++i) {
                composer.SetPendingInput(buffers[i].trigger);

                // Prevent the app from receiving our keyboard input
                buffers[i].trigger = 0;
                buffers[i].hold    = 0;
                buffers[i].release = 0;
            }
        }
    }

    if (error) {
        *error = realError;
    }

    return result;
}

WUPS_MUST_REPLACE(GX2SetContextState_hook, WUPS_LOADER_LIBRARY_GX2, GX2SetContextState);
WUPS_MUST_REPLACE(GX2SetupContextStateEx_hook, WUPS_LOADER_LIBRARY_GX2, GX2SetupContextStateEx);
WUPS_MUST_REPLACE(GX2Init_hook, WUPS_LOADER_LIBRARY_GX2, GX2Init);
WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer_hook, WUPS_LOADER_LIBRARY_GX2, GX2CopyColorBufferToScanBuffer);
WUPS_MUST_REPLACE(GX2SwapScanBuffers_hook, WUPS_LOADER_LIBRARY_GX2, GX2SwapScanBuffers);

WUPS_MUST_REPLACE(VPADRead_hook, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
