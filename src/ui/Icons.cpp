#include "ui/Icons.h"
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace mosaic::ui::icons {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Box {
    float cx, cy, half; // center + half-extent of the largest inscribed square
};

Box FitSquare(D2D1_RECT_F bounds) {
    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    float half = (w < h ? w : h) * 0.5f;
    return { bounds.left + w * 0.5f, bounds.top + h * 0.5f, half };
}

void DrawLeaf(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    // A leaf as two mirrored arcs meeting at a point, plus a short stem.
    // Simpler and cheaper than importing an actual leaf glyph, and reads
    // clearly at the ~12px size it's used at next to the greeting text.
    Box b = FitSquare(bounds);
    float r = b.half * 0.85f;

    ComPtr<ID2D1PathGeometry> geo;
    ComPtr<ID2D1Factory> factory;
    ctx->GetFactory(&factory);
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);

    D2D1_POINT_2F top = { b.cx, b.cy - r };
    D2D1_POINT_2F bottom = { b.cx, b.cy + r * 0.9f };

    sink->BeginFigure(top, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc(D2D1::ArcSegment(bottom, { r * 0.7f, r * 1.1f }, 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddArc(D2D1::ArcSegment(top, { r * 0.7f, r * 1.1f }, 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    brush->SetColor(color);
    ctx->FillGeometry(geo.Get(), brush);
}

void DrawGear(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);
    float outerR = b.half * 0.85f;
    float innerR = b.half * 0.55f;
    float holeR = b.half * 0.30f;
    const int teeth = 8;

    brush->SetColor(color);

    D2D1_MATRIX_3X2_F original;
    ctx->GetTransform(&original);

    for (int i = 0; i < teeth; ++i) {
        float angle = (2.0f * kPi * i) / teeth;
        ctx->SetTransform(D2D1::Matrix3x2F::Rotation(angle * 180.0f / kPi, { b.cx, b.cy }) * original);
        D2D1_RECT_F tooth = {
            b.cx - b.half * 0.10f, b.cy - outerR,
            b.cx + b.half * 0.10f, b.cy - innerR
        };
        ctx->FillRectangle(tooth, brush);
    }
    ctx->SetTransform(original);

    // Body ring + center hole (hole "cut" by drawing background-colored
    // circle would require knowing the background color; instead we just
    // stroke a ring, which reads correctly against any card fill).
    ctx->DrawEllipse(D2D1::Ellipse({ b.cx, b.cy }, innerR, innerR), brush, b.half * 0.16f);
    ctx->DrawEllipse(D2D1::Ellipse({ b.cx, b.cy }, holeR, holeR), brush, b.half * 0.10f);
}

void DrawStar(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);
    float outerR = b.half * 0.95f;
    float innerR = outerR * 0.42f;

    ComPtr<ID2D1PathGeometry> geo;
    ComPtr<ID2D1Factory> factory;
    ctx->GetFactory(&factory);
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);

    const int points = 5;
    for (int i = 0; i < points * 2; ++i) {
        float r = (i % 2 == 0) ? outerR : innerR;
        // -90 degrees so the first point aims straight up.
        float angle = (kPi * i / points) - (kPi / 2.0f);
        D2D1_POINT_2F p = { b.cx + r * std::cos(angle), b.cy + r * std::sin(angle) };
        if (i == 0) sink->BeginFigure(p, D2D1_FIGURE_BEGIN_FILLED);
        else        sink->AddLine(p);
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    brush->SetColor(color);
    ctx->FillGeometry(geo.Get(), brush);
}

void DrawPin(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);
    float r = b.half * 0.6f;
    float tipY = b.cy + b.half * 0.95f;

    ComPtr<ID2D1PathGeometry> geo;
    ComPtr<ID2D1Factory> factory;
    ctx->GetFactory(&factory);
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);

    // Rounded teardrop: arc across the top ~300 degrees, then two straight
    // edges converging to a point below — the classic map-pin silhouette.
    float startAngle = 210.0f * kPi / 180.0f;
    float endAngle = -30.0f * kPi / 180.0f;
    D2D1_POINT_2F start = { b.cx + r * std::cos(startAngle), b.cy - b.half * 0.15f + r * std::sin(startAngle) };
    D2D1_POINT_2F end   = { b.cx + r * std::cos(endAngle),   b.cy - b.half * 0.15f + r * std::sin(endAngle) };
    D2D1_POINT_2F tip    = { b.cx, tipY };

    sink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc(D2D1::ArcSegment(end, { r, r }, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
    sink->AddLine(tip);
    sink->AddLine(start);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    brush->SetColor(color);
    ctx->FillGeometry(geo.Get(), brush);
}

void DrawLock(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);

    // Shackle: an open arc above the body.
    float shackleR = b.half * 0.45f;
    D2D1_POINT_2F shackleTop = { b.cx, b.cy - b.half * 0.15f };
    D2D1_POINT_2F left  = { b.cx - shackleR, shackleTop.y };
    D2D1_POINT_2F right = { b.cx + shackleR, shackleTop.y };

    ComPtr<ID2D1PathGeometry> shackle;
    ComPtr<ID2D1Factory> factory;
    ctx->GetFactory(&factory);
    factory->CreatePathGeometry(&shackle);
    ComPtr<ID2D1GeometrySink> shackleSink;
    shackle->Open(&shackleSink);
    shackleSink->BeginFigure(left, D2D1_FIGURE_BEGIN_HOLLOW);
    shackleSink->AddArc(D2D1::ArcSegment(right, { shackleR, shackleR }, 0.0f,
                                         D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    shackleSink->EndFigure(D2D1_FIGURE_END_OPEN);
    shackleSink->Close();

    brush->SetColor(color);
    ctx->DrawGeometry(shackle.Get(), brush, b.half * 0.14f);

    // Body: rounded rect below the shackle.
    D2D1_RECT_F bodyRect = {
        b.cx - b.half * 0.75f, b.cy - b.half * 0.05f,
        b.cx + b.half * 0.75f, b.cy + b.half * 0.85f
    };
    D2D1_ROUNDED_RECT body = D2D1::RoundedRect(bodyRect, b.half * 0.18f, b.half * 0.18f);
    ctx->FillRoundedRectangle(body, brush);
}

void DrawCheck(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);
    float s = b.half;

    D2D1_POINT_2F p1 = { b.cx - s * 0.55f, b.cy + s * 0.05f };
    D2D1_POINT_2F p2 = { b.cx - s * 0.12f, b.cy + s * 0.40f };
    D2D1_POINT_2F p3 = { b.cx + s * 0.60f, b.cy - s * 0.40f };

    brush->SetColor(color);
    float strokeWidth = s * 0.28f;
    ctx->DrawLine(p1, p2, brush, strokeWidth);
    ctx->DrawLine(p2, p3, brush, strokeWidth);
}

void DrawChevronRight(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    Box b = FitSquare(bounds);
    float s = b.half;
    D2D1_POINT_2F p1 = { b.cx - s * 0.25f, b.cy - s * 0.55f };
    D2D1_POINT_2F p2 = { b.cx + s * 0.35f, b.cy };
    D2D1_POINT_2F p3 = { b.cx - s * 0.25f, b.cy + s * 0.55f };

    brush->SetColor(color);
    float strokeWidth = s * 0.22f;
    ctx->DrawLine(p1, p2, brush, strokeWidth);
    ctx->DrawLine(p2, p3, brush, strokeWidth);
}

} // namespace

void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush,
          IconKind kind, D2D1_RECT_F bounds, D2D1_COLOR_F color) {
    switch (kind) {
    case IconKind::Leaf:         DrawLeaf(ctx, brush, bounds, color); break;
    case IconKind::Gear:         DrawGear(ctx, brush, bounds, color); break;
    case IconKind::Star:         DrawStar(ctx, brush, bounds, color); break;
    case IconKind::Pin:          DrawPin(ctx, brush, bounds, color); break;
    case IconKind::Lock:         DrawLock(ctx, brush, bounds, color); break;
    case IconKind::Check:        DrawCheck(ctx, brush, bounds, color); break;
    case IconKind::ChevronRight: DrawChevronRight(ctx, brush, bounds, color); break;
    case IconKind::None: default: break;
    }
}

} // namespace mosaic::ui::icons
