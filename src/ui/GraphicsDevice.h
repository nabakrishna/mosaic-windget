#pragma once
#include <windows.h>
#include <d3d11_1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <wrl/client.h>

namespace mosaic::ui {

// Owns the full rendering device chain for one top-level window:
//
//   D3D11 device (BGRA-enabled, for D2D interop)
//     -> DXGI device
//       -> DXGI swap chain (CreateSwapChainForComposition — no HWND target;
//          DirectComposition owns presentation, not DWM's classic model)
//         -> D2D1 device -> D2D1 device context (our actual draw surface)
//       -> DirectComposition device -> target -> visual (binds swap chain
//          to the HWND's composition tree)
//
// This is the standard "DirectComposition + Direct2D" pattern for windows
// that need real alpha transparency with sharp, low-overhead 2D drawing —
// no DXGI_SWAP_EFFECT_DISCARD full-window blits, no browser compositor.
class GraphicsDevice {
public:
    GraphicsDevice() = default;
    ~GraphicsDevice() = default;

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Creates every device object and binds the composition visual to `hwnd`.
    // `hwnd` must have been created with the WS_EX_NOREDIRECTIONBITMAP
    // extended style (see Window::Create) so DWM doesn't allocate its own
    // redirection surface behind our swap chain.
    HRESULT Initialize(HWND hwnd, UINT pixelWidth, UINT pixelHeight);

    // Resizes the swap chain buffers. Call from WM_SIZE. No-op if the size
    // is unchanged (resizing a swap chain is not free).
    HRESULT Resize(UINT pixelWidth, UINT pixelHeight);

    // Begin/End a frame. Between these two calls, use DeviceContext() to
    // issue D2D drawing commands. EndDraw presents via the DXGI swap chain
    // AND commits the DirectComposition tree (both are required).
    void BeginDraw();
    HRESULT EndDraw();

    ID2D1DeviceContext* DeviceContext() const { return m_d2dContext.Get(); }
    IDWriteFactory*     DWriteFactory()  const { return m_dwriteFactory.Get(); }

private:
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources(HWND hwnd, UINT pixelWidth, UINT pixelHeight);
    HRESULT CreateSwapChainBitmap();

    Microsoft::WRL::ComPtr<ID3D11Device>          m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>   m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGIDevice>           m_dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIFactory2>         m_dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>       m_swapChain;

    Microsoft::WRL::ComPtr<ID2D1Factory1>         m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device>           m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext>    m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1>          m_targetBitmap;

    Microsoft::WRL::ComPtr<IDWriteFactory>        m_dwriteFactory;

    Microsoft::WRL::ComPtr<IDCompositionDevice>   m_compDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget>   m_compTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual>   m_compVisual;

    HWND m_hwnd = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
};

} // namespace mosaic::ui
