#pragma once

#include "transaction/transaction.h"
#include <cstdint>
#include <string>
#include <vector>

namespace blockchain {

/**
 * Represents a single block in the blockchain.
 * Contains a set of transactions and a Proof-of-Work nonce.
 */
struct Block {
  uint64_t index;
  std::string timestamp; // ISO 8601
  std::string previousHash;
  std::vector<Transaction> transactions;
  std::string merkleRoot;
  uint64_t nonce;
  uint32_t difficulty;
  std::string hash;

  /** Default constructor. */
  Block();

  /**
   * Construct a block with the given parameters.
   */
  Block(uint64_t index, const std::string &previousHash,
        const std::vector<Transaction> &transactions, uint32_t difficulty);

  /** Compute the Merkle root from the block's transactions. */
  std::string computeMerkleRoot() const;

  /** Calculate the block hash from all fields. */
  std::string calculateHash() const;

  /**
   * Mine this block: increment nonce until hash has 'difficulty' leading zeros.
   */
  void mineBlock();

  /**
   * Validate that the block hash matches its contents
   * and the difficulty target is met.
   */
  bool isValid() const;

  /**
   * Validate all transactions in this block.
   * @return true if all transactions have valid signatures
   */
  bool hasValidTransactions() const;

  /** Serialize block to JSON. */
  nlohmann::json toJson() const;

  /** Deserialize block from JSON. */
  static Block fromJson(const nlohmann::json &j);

  /** Create the genesis block. */
  static Block createGenesis(uint32_t difficulty);
};

} // namespace blockchain
