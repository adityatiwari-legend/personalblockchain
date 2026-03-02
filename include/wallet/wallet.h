#pragma once

#include "crypto/ecdsa.h"
#include <string>

namespace blockchain {

/**
 * Wallet class for managing ECDSA key pairs and signing operations.
 * Each wallet represents an identity on the blockchain.
 */
class Wallet {
public:
  /**
   * Create a new wallet with a fresh key pair.
   */
  Wallet();

  /**
   * Restore a wallet from an existing private key.
   */
  explicit Wallet(const std::string &privateKeyHex);

  /** Get the public key (blockchain address). */
  const std::string &getPublicKey() const { return publicKey_; }

  /** Get the private key (keep secret!). */
  const std::string &getPrivateKey() const { return privateKey_; }

  /**
   * Sign arbitrary data with this wallet's private key.
   * @param data The data to sign
   * @return Hex-encoded ECDSA signature
   */
  std::string signData(const std::string &data) const;

  /** Serialize wallet public info to JSON string. */
  std::string toJson() const;

private:
  std::string privateKey_;
  std::string publicKey_;
};

} // namespace blockchain
