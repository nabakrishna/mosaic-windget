#include "ui/GraphicsDevice.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

namespace mosaic::ui {

HRESULT GraphicsDevice::Initialize(HWND hwnd, UINT pixelWidth, UINT pixelHeight) {
    m_hwnd = hwnd;

    HRESULT hr = CreateDeviceIndependentResources();
    if (FAILED(hr)) return hr;

    return CreateDeviceResources(hwnd, pixelWidth, pixelHeight);
}

HRESULT GraphicsDevice::CreateDeviceIndependentResources() {
    D2D1_FACTORY_OPTIONS options{};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED, // one UI thread does all drawing
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
    if (FAILED(hr)) return hr;

    return DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
}

HRESULT GraphicsDevice::CreateDeviceResources(HWND hwnd, UINT pixelWidth, UINT pixelHeight) {
    // --- D3D11 device (BGRA support is required for D2D interop) --------
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
    };
    D3D_FEATURE_LEVEL achievedLevel{};

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_d3dDevice, &achievedLevel, &m_d3dContext);
    if (FAILED(hr)) return hr;

    hr = m_d3dDevice.As(&m_dxgiDevice);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = m_dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return hr;

    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr)) return hr;

    // --- Swap chain for composition (NOT CreateSwapChainForHwnd) ---------
    // This is the key difference from a normal DWM-presented window: the
    // swap chain has no target window. DirectComposition is what actually
    // presents it, which is what lets us layer transparent, GPU-drawn
    // content over the desktop's own acrylic/mica backdrop with no tearing
    // and no extra composition hop.
    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Width = pixelWidth;
    scDesc.Height = pixelHeight;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.Stereo = FALSE;
    scDesc.SampleDesc = { 1, 0 };
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // real per-pixel alpha
    scDesc.Flags = 0;

    hr = m_dxgiFactory->CreateSwapChainForComposition(
        m_dxgiDevice.Get(), &scDesc, nullptr, &m_swapChain);
    if (FAILED(hr)) return hr;

    // --- D2D device + context on top of the same D3D device --------------
    hr = m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr)) return hr;

    hr = m_d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    if (FAILED(hr)) return hr;

    hr = CreateSwapChainBitmap();
    if (FAILED(hr)) return hr;

    // --- DirectComposition: device -> target -> visual -> swap chain -----
    hr = DCompositionCreateDevice(
        m_dxgiDevice.Get(), IID_PPV_ARGS(&m_compDevice));
    if (FAILED(hr)) return hr;

    hr = m_compDevice->CreateTargetForHwnd(hwnd, TRUE /*topmost within tree*/, &m_compTarget);
    if (FAILED(hr)) return hr;

    hr = m_compDevice->CreateVisual(&m_compVisual);
    if (FAILED(hr)) return hr;

    hr = m_compVisual->SetContent(m_swapChain.Get());
    if (FAILED(hr)) return hr;

    hr = m_compTarget->SetRoot(m_compVisual.Get());
    if (FAILED(hr)) return hr;

    hr = m_compDevice->Commit();
    if (FAILED(hr)) return hr;

    m_width = pixelWidth;
    m_height = pixelHeight;
    return S_OK;
}

HRESULT GraphicsDevice::CreateSwapChainBitmap() {
    ComPtr<IDXGISurface> surface;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &m_targetBitmap);
    if (FAILED(hr)) return hr;

    m_d2dContext->SetTarget(m_targetBitmap.Get());
    return S_OK;
}

HRESULT GraphicsDevice::Resize(UINT pixelWidth, UINT pixelHeight) {
    if (pixelWidth == 0 || pixelHeight == 0) return S_OK; // minimized
    if (pixelWidth == m_width && pixelHeight == m_height) return S_OK;

    // Must release the target bitmap (and anything else referencing the
    // swap chain's buffers) before calling ResizeBuffers.
    m_d2dContext->SetTarget(nullptr);
    m_targetBitmap.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(
        0, pixelWidth, pixelHeight, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return hr;

    m_width = pixelWidth;
    m_height = pixelHeight;
    return CreateSwapChainBitmap();
}

void GraphicsDevice::BeginDraw() {
    m_d2dContext->BeginDraw();
}

HRESULT GraphicsDevice::EndDraw() {
    HRESULT hr = m_d2dContext->EndDraw();
    if (FAILED(hr)) return hr;

    DXGI_PRESENT_PARAMETERS presentParams{};
    hr = m_swapChain->Present1(1 /*vsync*/, 0, &presentParams);
    if (FAILED(hr)) return hr;

    // DirectComposition batches visual-tree changes; Commit() flushes them.
    // We only actually need this after structural changes (SetContent etc.)
    // but calling it is cheap and keeps the tree consistent defensively.
    return m_compDevice->Commit();
}

} // namespace mosaic::ui
