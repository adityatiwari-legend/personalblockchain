#pragma once

#include <string>
#include <utility>
#include <vector>


namespace blockchain {
namespace crypto {

/**
 * Represents an ECDSA key pair (secp256k1 curve).
 */
struct KeyPair {
  std::string privateKey; // Hex-encoded private key
  std::string publicKey;  // Hex-encoded compressed public key
};

/**
 * Generate a new ECDSA key pair on the secp256k1 curve.
 * @return KeyPair with hex-encoded private and public keys
 */
KeyPair generateKeyPair();

/**
 * Sign a message using ECDSA with the given private key.
 * @param message The message to sign (will be SHA-256 hashed internally)
 * @param privateKeyHex Hex-encoded private key
 * @return Hex-encoded DER signature
 */
std::string sign(const std::string &message, const std::string &privateKeyHex);

/**
 * Verify an ECDSA signature.
 * @param message The original message
 * @param signatureHex Hex-encoded DER signature
 * @param publicKeyHex Hex-encoded compressed public key
 * @return true if signature is valid
 */
bool verify(const std::string &message, const std::string &signatureHex,
            const std::string &publicKeyHex);

/**
 * Convert a hex string to bytes.
 */
std::vector<unsigned char> hexToBytes(const std::string &hex);

/**
 * Convert bytes to a hex string.
 */
std::string bytesToHex(const std::vector<unsigned char> &bytes);

} // namespace crypto
} // namespace blockchain
