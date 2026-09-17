#include "widgets/PhotoWidget.h"
#include "ui/components/GlassPanel.h"
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

namespace {
// Fixed decode target. A real per-card/per-DPI target would be nicer, but
// would mean re-decoding on every resize — a needless decode for a purely
// cosmetic aspect-ratio match. This is a documented simplification, not an
// oversight; Settings' cache-size control (Phase 7) is the natural place
// to make this configurable if it ever needs to be.
constexpr UINT kTargetWidth = 960;
constexpr UINT kTargetHeight = 600;

constexpr ULONGLONG kTransitionDurationMs = 220; // spec section 10: "approximately 150-300ms"
} // namespace

PhotoWidget::PhotoWidget(media::IPhotoProvider* provider, media::ImagePipeline* pipeline)
    : m_provider(provider), m_pipeline(pipeline) {}

void PhotoWidget::RequestNextPhoto(ID2D1DeviceContext* ctx) {
    if (!m_provider->HasPhotos()) {
        m_noPhotosAvailable = true;
        return;
    }
    m_noPhotosAvailable = false;

    auto path = m_provider->NextPhotoPath();
    if (!path) return;

    m_pipeline->RequestDecode(*path, kTargetWidth, kTargetHeight,
        [this, ctx](media::DecodedImage image) {
            OnPhotoDecoded(std::move(image), ctx);
        });
}

void PhotoWidget::OnPhotoDecoded(media::DecodedImage image, ID2D1DeviceContext* ctx) {
    if (!image.success || image.pixels.empty() || !ctx) {
        // Corrupt or unreadable file — spec section 60: "skip it," keep
        // showing whatever is already on screen rather than clearing it.
        return;
    }

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap> newBitmap;
    HRESULT hr = ctx->CreateBitmap(
        D2D1::SizeU(image.width, image.height), image.pixels.data(),
        image.width * 4, &props, &newBitmap);
    // `image.pixels` — the only sizeable CPU-side buffer in this whole
    // path — goes out of scope at the end of this function either way.
    // Nothing holds a reference to it past this point.

    if (FAILED(hr) || !newBitmap) return; // keep showing whatever we had

    // The brief exception spec section 9 itself allows: both textures are
    // alive simultaneously only for the crossfade's ~220ms, not "a long
    // period" — and both are already bounded to kTargetWidth x
    // kTargetHeight, roughly 2.3MB each, ~4.6MB total at the peak moment.
    m_previousTexture = m_currentTexture;
    m_currentTexture = newBitmap;
    m_transitioning = (m_previousTexture != nullptr); // first-ever photo has nothing to fade from
    m_transitionStartTick = GetTickCount64();
}

void PhotoWidget::OnDeviceLost() {
    // Both textures belong to the destroyed device — holding onto them
    // would mean drawing with resources tied to a dead device on the next
    // frame. Window triggers a fresh RequestNextPhoto right after
    // recovering, so this is a brief, self-healing gap, not a lasting one.
    m_currentTexture.Reset();
    m_previousTexture.Reset();
    m_transitioning = false;
}

void PhotoWidget::DrawPlaceholder(ID2D1DeviceContext* ctx, const WidgetRenderResources& res,
                                   D2D1_RECT_F bounds, const wchar_t* message) {
    const ThemeMetrics& metrics = res.theme->Metrics();
    const ThemeColors& colors = res.theme->Colors();
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(bounds, metrics.cardCornerRadius, metrics.cardCornerRadius);

    D2D1_GRADIENT_STOP stops[3] = {
        { 0.0f, { 0.10f, 0.10f, 0.22f, 1.0f } },
        { 0.55f, { 0.16f, 0.14f, 0.30f, 1.0f } },
        { 1.0f, { 0.45f, 0.30f, 0.35f, 1.0f } },
    };
    ComPtr<ID2D1GradientStopCollection> stopCollection;
    ctx->CreateGradientStopCollection(stops, 3, &stopCollection);

    ComPtr<ID2D1LinearGradientBrush> gradientBrush;
    ctx->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties({ bounds.left, bounds.bottom }, { bounds.right, bounds.top }),
        stopCollection.Get(), &gradientBrush);

    ctx->FillRoundedRectangle(rr, gradientBrush.Get());

    D2D1_RECT_F textRect = bounds;
    res.brush->SetColor(colors.textSecondary);
    ctx->DrawText(message, static_cast<UINT32>(wcslen(message)), res.smallFormat, textRect, res.brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void PhotoWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeMetrics& metrics = res.theme->Metrics();

    GlassPanel::Draw(ctx, res.brush, bounds, *res.theme);

    if (!m_currentTexture) {
        DrawPlaceholder(ctx, res, bounds, m_noPhotosAvailable ? L"No photos found" : L"Loading photo\u2026");
        return;
    }

    // Clip both bitmap draws to the card's rounded-corner geometry — a
    // plain DrawBitmap would paint a square-cornered image poking past the
    // glass card's rounded corners otherwise.
    ComPtr<ID2D1Factory> factory;
    ctx->GetFactory(&factory); // out-parameter method, not a return value
    ComPtr<ID2D1RoundedRectangleGeometry> clipGeometry;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(bounds, metrics.cardCornerRadius, metrics.cardCornerRadius);
    factory->CreateRoundedRectangleGeometry(rr, &clipGeometry);

    ctx->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clipGeometry.Get()), nullptr);

    if (m_transitioning && m_previousTexture) {
        ULONGLONG elapsed = GetTickCount64() - m_transitionStartTick;
        float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(kTransitionDurationMs), 0.0f, 1.0f);

        ctx->DrawBitmap(m_previousTexture.Get(), &bounds, 1.0f,
                         D2D1_INTERPOLATION_MODE_LINEAR, nullptr);
        ctx->DrawBitmap(m_currentTexture.Get(), &bounds, t,
                         D2D1_INTERPOLATION_MODE_LINEAR, nullptr);

        if (elapsed >= kTransitionDurationMs) {
            // Transition finished — release the outgoing texture now
            // rather than waiting for the next photo change, per spec
            // section 9's "release unused textures immediately."
            m_previousTexture.Reset();
            m_transitioning = false;
        }
    } else {
        ctx->DrawBitmap(m_currentTexture.Get(), &bounds, 1.0f,
                         D2D1_INTERPOLATION_MODE_LINEAR, nullptr);
    }

    ctx->PopLayer();
}

} // namespace mosaic::widgets
