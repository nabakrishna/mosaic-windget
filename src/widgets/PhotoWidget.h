#pragma once
#include <d2d1_1.h>
#include <wrl/client.h>
#include "widgets/IWidget.h"
#include "media/IPhotoProvider.h"
#include "media/ImagePipeline.h"

namespace mosaic::widgets {

// The real photo pipeline (Phase 5), replacing the static gradient
// placeholder. Decode happens entirely off the UI thread via
// media::ImagePipeline; this class only ever touches the GPU on the UI
// thread (Render, and the decode-complete callback Window invokes from its
// message pump — see ImagePipeline.h's comment on why that's safe).
//
// Photo source and rotation interval are hardcoded today (the machine's
// Pictures folder, 5-minute default) — spec section 21's Photo & Media
// Settings (folder picker, interval dropdown, shuffle toggle, cache size)
// is Phase 7's job. What's real right now is everything Settings would
// eventually control the *behavior* of: the decode pipeline, the memory
// bounds, and the crossfade.
class PhotoWidget : public IWidget {
public:
    PhotoWidget(media::IPhotoProvider* provider, media::ImagePipeline* pipeline);

    WidgetId Id() const override { return WidgetId::Photo; }
    WidgetMetadata Metadata() const override {
        // return WidgetMetadata{ WidgetId::Photo, L"Photo", { 2, 2 }, { 1, 1 }, { 3, 3 } };
        // Change preferred to 3x3, and max to 6x6
        return WidgetMetadata{ WidgetId::Photo, L"Photo", { 4, 5 }, { 2, 2 }, { 6, 6 } };
    }

    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;
    void OnDeviceLost() override;

    // Called by Window on the rotation schedule (default every 5 minutes,
    // and once immediately at startup) and once right after device-lost
    // recovery. `ctx` is the *current* device context — passed in rather
    // than cached, since GraphicsDevice can be replaced wholesale on
    // device loss and a stale cached pointer would be exactly the bug
    // OnDeviceLost exists to avoid.
    void RequestNextPhoto(ID2D1DeviceContext* ctx);

    // True while the crossfade animation is still running. Window uses
    // this to decide whether to keep its short per-frame animation timer
    // alive or stop it — see Window.h's comment on the two-timer design.
    bool IsTransitioning() const { return m_transitioning; }

private:
    void OnPhotoDecoded(media::DecodedImage image, ID2D1DeviceContext* ctx);
    void DrawPlaceholder(ID2D1DeviceContext* ctx, const WidgetRenderResources& res,
                          D2D1_RECT_F bounds, const wchar_t* message);

    media::IPhotoProvider* m_provider;
    media::ImagePipeline* m_pipeline;

    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_currentTexture;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_previousTexture; // only alive during a crossfade

    bool m_transitioning = false;
    ULONGLONG m_transitionStartTick = 0;
    bool m_noPhotosAvailable = false;
};

} // namespace mosaic::widgets
