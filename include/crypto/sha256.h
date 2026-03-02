#pragma once

#include <string>

namespace blockchain {
namespace crypto {

/**
 * Computes SHA-256 hash of the input data.
 * @param data Input string to hash
 * @return Lowercase hex-encoded SHA-256 digest (64 characters)
 */
std::string sha256(const std::string& data);

/**
 * Computes double SHA-256 hash (SHA256(SHA256(data))).
 * Used for additional security in critical hashing operations.
 */
std::string doubleSha256(const std::string& data);

} // namespace crypto
} // namespace blockchain
