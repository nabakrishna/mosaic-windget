#pragma once
#include <string>
#include <optional>

namespace mosaic::media {

// Abstracts "where the next photo comes from" so PhotoWidget never knows or
// cares whether it's a local folder, and so a future permitted online
// source (spec section 7's OnlineFeedPhotoProvider) can be added later
// without touching PhotoWidget or the decode pipeline at all — only a new
// class implementing this interface, plus a registry entry.
//
// Deliberately NOT implemented in Phase 5: any online/Pinterest provider.
// The spec is explicit that this must go through an official API or a
// permitted feed, not scraping — no such integration exists yet to wire
// up honestly, so it doesn't exist here as a stub pretending otherwise.
class IPhotoProvider {
public:
    virtual ~IPhotoProvider() = default;

    // True once at least one photo has been found. PhotoWidget uses this
    // to distinguish "still scanning" / "genuinely empty" from "has photos
    // but decode of the current one failed" (spec section 60's distinct
    // "folder unavailable" vs. "corrupt image, skip it" error paths).
    virtual bool HasPhotos() const = 0;

    // Returns the path to show next, or nullopt if HasPhotos() is false.
    // Advances internal rotation state (e.g. shuffled index) as a side
    // effect — this is "give me what's next," not a peek.
    virtual std::optional<std::wstring> NextPhotoPath() = 0;
};

} // namespace mosaic::media
