#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace mosaic::security {

// Encrypts and decrypts Quick Notes content at rest, and verifies the
// user's password — without ever storing the password itself, and without
// any encryption key living in source code (spec section 39's explicit
// requirements).
//
// How this works, and why:
//
// ENCRYPTION uses Windows DPAPI (CryptProtectData/CryptUnprotectData) with
// CRYPTPROTECT_UI_FORBIDDEN. DPAPI derives its key from the logged-in
// Windows user's credentials, managed by the OS — so there is no key in
// this binary, no key in the database, and nothing for us to lose or leak.
// A copy of mosaic.db taken to another machine, or opened under a
// different Windows account, cannot be decrypted. That's exactly the
// property spec section 39 asks for, achieved by delegating to the OS
// rather than by hand-rolling crypto, which is the one thing an
// application should never do.
//
// THE PASSWORD is a separate concern from the encryption key, and it's
// worth being precise about what it does and doesn't do. The password
// gates *access in the UI*; DPAPI protects the data *on disk*. We never
// store the password — only a random salt plus a PBKDF2-SHA256 hash of
// (salt + password), and verification recomputes that hash and compares.
// So a stolen database yields neither the notes (DPAPI-protected) nor the
// password (only a salted slow hash of it).
//
// WHAT THIS DOES NOT CLAIM: because DPAPI is keyed to the Windows account
// rather than to the password, someone already logged into this Windows
// account with the ability to run arbitrary code could decrypt the notes
// without knowing the password. Defending against that would require
// deriving the encryption key from the password itself, which in turn
// means an unrecoverable loss if the password is forgotten. For a desktop
// notes widget, DPAPI is the right trade — but the limitation is real, so
// it's documented here rather than glossed over.
class NoteCrypto {
public:
    // Encrypts UTF-16 plaintext to an opaque blob. Returns empty on
    // failure — callers must treat empty as "do not overwrite stored
    // data", never as "store nothing".
    static std::vector<uint8_t> Encrypt(const std::wstring& plaintext);

    // Decrypts a blob produced by Encrypt. `outPlaintext` is left
    // untouched and false returned if the blob is corrupt, was produced
    // by another Windows user, or DPAPI otherwise refuses it.
    static bool Decrypt(const std::vector<uint8_t>& blob, std::wstring& outPlaintext);

    // Generates a fresh random salt (CNG's RNG, not rand()) and returns
    // the PBKDF2-SHA256 hash of the password with it. Both are stored;
    // neither reveals the password.
    static bool HashPassword(const std::wstring& password,
                              std::vector<uint8_t>& outSalt,
                              std::vector<uint8_t>& outHash);

    // Recomputes the hash of `password` with the stored `salt` and
    // compares it against `expectedHash` in constant time.
    static bool VerifyPassword(const std::wstring& password,
                                const std::vector<uint8_t>& salt,
                                const std::vector<uint8_t>& expectedHash);

    // Overwrites a wstring's buffer before it goes out of scope. Not a
    // guarantee (the compiler may have copied it, the OS may have paged
    // it), but it meaningfully shortens how long a plaintext password
    // sits in memory — spec section 39's "secure deletion where
    // practical", with the "where practical" taken seriously rather than
    // claimed as absolute.
    static void SecureWipe(std::wstring& text);
};

} // namespace mosaic::security
