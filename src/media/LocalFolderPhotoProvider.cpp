#include "media/LocalFolderPhotoProvider.h"
#include <windows.h>
#include <algorithm>
#include <random>

namespace mosaic::media {

namespace {

bool HasSupportedExtension(const std::wstring& filename) {
    size_t dot = filename.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext == L"jpg" || ext == L"jpeg" || ext == L"png" ||
           ext == L"webp" || ext == L"bmp";
}

// Recursively walks `dir`, appending matching file paths to `outFiles`.
// This is pure filesystem-metadata enumeration (FindFirstFile/FindNextFile)
// — no file content is opened or read here, only names and the
// FILE_ATTRIBUTE_DIRECTORY bit, which is what keeps this "lazy" per the
// class comment: even a folder with thousands of photos costs only a
// FindNextFile call per entry, not a decode.
void ScanRecursive(const std::wstring& dir, std::vector<std::wstring>& outFiles, int depthRemaining) {
    if (depthRemaining <= 0) return; // guards against pathological symlink loops

    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring fullPath = dir + L"\\" + name;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanRecursive(fullPath, outFiles, depthRemaining - 1);
        } else if (HasSupportedExtension(name)) {
            outFiles.push_back(fullPath);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

} // namespace

void LocalFolderPhotoProvider::SetFolder(const std::wstring& folderPath) {
    m_files.clear();
    m_cursor = 0;
    ScanRecursive(folderPath, m_files, /*depthRemaining*/ 8);
    Reshuffle();
}

void LocalFolderPhotoProvider::Reshuffle() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(m_files.begin(), m_files.end(), gen);
}

std::optional<std::wstring> LocalFolderPhotoProvider::NextPhotoPath() {
    if (m_files.empty()) return std::nullopt;

    if (m_cursor >= m_files.size()) {
        // Exhausted this pass — reshuffle so the next full cycle isn't in
        // the exact same order, then start over.
        Reshuffle();
        m_cursor = 0;
    }

    return m_files[m_cursor++];
}

} // namespace mosaic::media
