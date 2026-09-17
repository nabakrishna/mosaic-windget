#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "data/Database.h"

namespace mosaic::data {

// Persistence for the single Quick Notes document. Everything stored goes
// through security::NoteCrypto first — this class never writes plaintext
// to the database, and never stores the password (only a salt and a
// PBKDF2 hash of it).
//
// It's a single note, not a list: the reference design shows one
// scratchpad ("Write something..."), and spec section 14 describes Quick
// Notes as "a private scratchpad" rather than a collection. One row,
// id = 1.
class NotesRepository {
public:
    explicit NotesRepository(Database* database) : m_database(database) {}

    // Returns false if there's nothing saved yet, or if decryption fails
    // (corrupt blob, or a database copied from a different Windows
    // account — see NoteCrypto.h on why that can't be decrypted).
    bool LoadNote(std::wstring& outPlaintext);

    // Encrypts and stores. Returns false if encryption failed, in which
    // case nothing is written — deliberately preferring to keep the last
    // good ciphertext over replacing it with something unreadable.
    bool SaveNote(const std::wstring& plaintext);

    bool HasPassword();

    // Replaces any existing password. Returns false if hashing failed.
    bool SetPassword(const std::wstring& password);

    // False if no password is set, or if it doesn't match.
    bool VerifyPassword(const std::wstring& password);

private:
    void EnsureRow();

    Database* m_database;
};

} // namespace mosaic::data
