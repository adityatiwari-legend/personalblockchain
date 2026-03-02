#pragma once

#include "utils/json.hpp"
#include <cstdint>
#include <string>


namespace blockchain {

/**
 * Represents a single transaction on the blockchain.
 * Each transaction is signed by the sender using ECDSA.
 */
struct Transaction {
  std::string txID; // SHA-256(sender + receiver + payload + timestamp)
  std::string senderPublicKey;   // Compressed hex public key of sender
  std::string receiverPublicKey; // Compressed hex public key of receiver
  std::string payload;           // Generic payload (JSON string or plaintext)
  std::string timestamp;         // ISO 8601 format
  std::string digitalSignature;  // Hex-encoded ECDSA signature of txID

  /** Compute the transaction ID from fields. */
  void computeTxID();

  /** Sign the transaction with the sender's private key. */
  void sign(const std::string &privateKey);

  /** Verify the digital signature against the sender's public key. */
  bool verify() const;

  /**
   * Check if transaction has expired (replay prevention).
   * @param windowSeconds Maximum age in seconds (default 5 minutes)
   * @return true if the transaction is too old
   */
  bool isExpired(int64_t windowSeconds = 300) const;

  /** Get current time as ISO 8601 string. */
  static std::string currentTimestamp();

  /** Serialize to JSON object. */
  nlohmann::json toJson() const;

  /** Deserialize from JSON object. */
  static Transaction fromJson(const nlohmann::json &j);

  /** Check if this is a coinbase (mining reward) transaction. */
  bool isCoinbase() const;
};

} // namespace blockchain
