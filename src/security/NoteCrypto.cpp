#include "security/NoteCrypto.h"
#include <windows.h>
#include <dpapi.h>
#include <bcrypt.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace mosaic::security {

namespace {

constexpr int kSaltBytes = 16;
constexpr int kHashBytes = 32;       // SHA-256 output
constexpr int kPbkdf2Iterations = 100000;

// RAII for a BCrypt algorithm provider — several early-return paths below
// would otherwise leak it.
struct ScopedAlgProvider {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~ScopedAlgProvider() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};

// DPAPI allocates its output with LocalAlloc; this guarantees LocalFree.
struct ScopedLocalBuffer {
    void* ptr = nullptr;
    ~ScopedLocalBuffer() { if (ptr) LocalFree(ptr); }
};

bool DeriveKey(const std::wstring& password, const std::vector<uint8_t>& salt,
               std::vector<uint8_t>& outHash) {
    ScopedAlgProvider alg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_SHA256_ALGORITHM,
                                                   nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status != 0) return false;

    // PBKDF2 operates on bytes; hash the UTF-16 representation directly
    // rather than converting to UTF-8 first — fewer copies of the
    // plaintext password in memory, and the encoding only needs to be
    // consistent between HashPassword and VerifyPassword, not portable.
    const auto* passwordBytes = reinterpret_cast<const UCHAR*>(password.c_str());
    ULONG passwordByteCount = static_cast<ULONG>(password.size() * sizeof(wchar_t));

    outHash.assign(kHashBytes, 0);
    status = BCryptDeriveKeyPBKDF2(
        alg.handle,
        const_cast<UCHAR*>(passwordBytes), passwordByteCount,
        const_cast<UCHAR*>(salt.data()), static_cast<ULONG>(salt.size()),
        kPbkdf2Iterations,
        outHash.data(), static_cast<ULONG>(outHash.size()),
        0);
    return status == 0;
}

} // namespace

std::vector<uint8_t> NoteCrypto::Encrypt(const std::wstring& plaintext) {
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(plaintext.c_str()));
    input.cbData = static_cast<DWORD>(plaintext.size() * sizeof(wchar_t));

    DATA_BLOB output{};
    // CRYPTPROTECT_UI_FORBIDDEN: never let DPAPI pop its own UI. This can
    // be called during a save on a timer; a surprise modal dialog from a
    // background save would be worse than a failed save the caller handles.
    BOOL ok = CryptProtectData(&input, L"Mosaic Quick Notes", nullptr, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok) return {};

    ScopedLocalBuffer guard{ output.pbData };
    return std::vector<uint8_t>(output.pbData, output.pbData + output.cbData);
}

bool NoteCrypto::Decrypt(const std::vector<uint8_t>& blob, std::wstring& outPlaintext) {
    if (blob.empty()) return false;

    DATA_BLOB input{};
    input.pbData = const_cast<BYTE*>(blob.data());
    input.cbData = static_cast<DWORD>(blob.size());

    DATA_BLOB output{};
    BOOL ok = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                  CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok) return false;

    ScopedLocalBuffer guard{ output.pbData };
    outPlaintext.assign(reinterpret_cast<wchar_t*>(output.pbData), output.cbData / sizeof(wchar_t));

    // Wipe the decrypted copy DPAPI handed back before releasing it —
    // otherwise plaintext lingers in freed heap memory.
    SecureZeroMemory(output.pbData, output.cbData);
    return true;
}

bool NoteCrypto::HashPassword(const std::wstring& password,
                               std::vector<uint8_t>& outSalt,
                               std::vector<uint8_t>& outHash) {
    outSalt.assign(kSaltBytes, 0);
    // BCryptGenRandom with the system-preferred RNG — a cryptographic
    // source, unlike rand(), which would make the salt predictable and
    // defeat its purpose.
    NTSTATUS status = BCryptGenRandom(nullptr, outSalt.data(), kSaltBytes,
                                       BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) return false;

    return DeriveKey(password, outSalt, outHash);
}

bool NoteCrypto::VerifyPassword(const std::wstring& password,
                                 const std::vector<uint8_t>& salt,
                                 const std::vector<uint8_t>& expectedHash) {
    if (salt.empty() || expectedHash.empty()) return false;

    std::vector<uint8_t> computed;
    if (!DeriveKey(password, salt, computed)) return false;
    if (computed.size() != expectedHash.size()) return false;

    // Constant-time compare: an early-exit memcmp leaks, via timing, how
    // many leading bytes matched. The cost of always comparing every byte
    // is nothing; the habit is worth keeping even where the threat is
    // remote.
    uint8_t difference = 0;
    for (size_t i = 0; i < computed.size(); ++i) {
        difference |= static_cast<uint8_t>(computed[i] ^ expectedHash[i]);
    }
    SecureZeroMemory(computed.data(), computed.size());
    return difference == 0;
}

void NoteCrypto::SecureWipe(std::wstring& text) {
    if (!text.empty()) {
        SecureZeroMemory(&text[0], text.size() * sizeof(wchar_t));
    }
    text.clear();
}

} // namespace mosaic::security
