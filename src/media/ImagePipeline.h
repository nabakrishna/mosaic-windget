#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <functional>

namespace mosaic::media {

// The custom message ImagePipeline posts to the owning window when a decode
// finishes. Window's WndProc forwards it to ImagePipeline::PumpResult,
// which is what actually invokes the caller's callback — always on the UI
// thread, never on the worker thread, so the callback can safely touch a
// D2D device context.
constexpr UINT WM_MOSAIC_PHOTO_READY = WM_APP + 1;

// One decoded photo, already downscaled to (at most) the requested display
// size and converted to a straight 32bpp premultiplied-BGRA buffer — ready
// for a single ID2D1DeviceContext::CreateBitmap call and nothing else.
struct DecodedImage {
    std::vector<uint8_t> pixels; // premultiplied BGRA, tightly packed (stride == width*4)
    UINT width = 0;
    UINT height = 0;
    bool success = false;
};

// Runs exactly one background thread for the process's lifetime, decoding
// one photo at a time. This is the entire answer to spec section 33's
// "never block the UI thread with network/image decode" and section 9's
// memory requirements:
//
//   RequestDecode (UI thread, returns immediately)
//     -> queued to the worker thread
//     -> worker: read file, WIC-decode, downscale toward the requested
//        display size (see ImagePipeline.cpp's DecodeAndScaleCover for the
//        exact strategy and its honestly-documented limits), produce one
//        small pixel buffer
//     -> PostMessage(hwnd, WM_MOSAIC_PHOTO_READY) wakes the UI thread
//     -> UI thread calls PumpResult(), which hands the buffer to the
//        caller's callback (PhotoWidget), which uploads it to a GPU
//        texture and lets the CPU-side buffer go out of scope immediately
//
// Only one request is ever in flight or queued: a newer RequestDecode
// supersedes whatever the worker is doing via a generation counter, so a
// slow decode of a photo nobody wants anymore can't land after a newer one
// (relevant if rotation interval is set very short in a later Settings
// pass, or the user forces a manual "next photo" — not exercised by Phase
// 5's fixed default interval today, but correct regardless).
class ImagePipeline {
public:
    using ResultCallback = std::function<void(DecodedImage)>;

    ImagePipeline();
    ~ImagePipeline();

    ImagePipeline(const ImagePipeline&) = delete;
    ImagePipeline& operator=(const ImagePipeline&) = delete;

    // Must be called once, after `hwnd` exists, before the first
    // RequestDecode. Starts the single worker thread.
    void Start(HWND hwnd);

    // Stops the worker thread. Safe to call from the destructor; blocks
    // briefly if a decode is mid-flight (bounded — a single image decode,
    // not a network wait).
    void Stop();

    // Queues a decode. `onComplete` is invoked later on the UI thread from
    // inside PumpResult — never synchronously, never on the worker thread.
    void RequestDecode(std::wstring filePath, UINT targetWidth, UINT targetHeight, ResultCallback onComplete);

    // Call from Window's WM_MOSAIC_PHOTO_READY handler. Invokes the
    // callback for the most recently completed decode, if any.
    void PumpResult();

private:
    void WorkerThreadMain();

    struct PendingResult {
        DecodedImage image;
        ResultCallback callback;
    };

    HWND m_hwnd = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_stopping{ false };

    // --- guarded by m_mutex ---
    std::mutex m_mutex;
    std::condition_variable m_cv;
    uint64_t m_generation = 0;          // bumped on every RequestDecode
    uint64_t m_requestedGeneration = 0; // which generation the worker should be working toward
    std::wstring m_requestedPath;
    UINT m_requestedWidth = 0;
    UINT m_requestedHeight = 0;
    ResultCallback m_requestedCallback;
    std::optional<PendingResult> m_pendingResult; // set by worker, consumed by PumpResult on UI thread
};

} // namespace mosaic::media
