#pragma once
#include <vector>
#include <string>
#include "media/IPhotoProvider.h"

namespace mosaic::media {

// Scans a folder tree (recursively — matches spec section 7's example of
// Pictures/Wallpapers/Mountain/Travel nesting) for supported image files.
// "Lazy discovery" here means exactly what spec section 7 asks for: this
// class only ever holds file *paths* (a few dozen bytes each) in memory,
// never pixel data — nothing is decoded until PhotoWidget actually asks
// the image pipeline to load a specific path returned by NextPhotoPath().
//
// Rotation order is shuffled once when the folder is (re)scanned, then
// consumed round-robin — this matches the reference design's default
// "Shuffle: on" while still guaranteeing every photo is shown once before
// any repeat, rather than pure-random repeats.
class LocalFolderPhotoProvider : public IPhotoProvider {
public:
    // (Re)scans `folderPath` and replaces the current file list. Safe to
    // call again later once Settings (Phase 7) exposes a folder picker —
    // nothing else about this class assumes the folder is fixed for the
    // process's lifetime.
    void SetFolder(const std::wstring& folderPath);

    bool HasPhotos() const override { return !m_files.empty(); }
    std::optional<std::wstring> NextPhotoPath() override;

private:
    void Reshuffle();

    std::vector<std::wstring> m_files;
    size_t m_cursor = 0;
};

} // namespace mosaic::media
