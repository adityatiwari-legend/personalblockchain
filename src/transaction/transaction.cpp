#include "transaction/transaction.h"
#include "crypto/ecdsa.h"
#include "crypto/sha256.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace blockchain
{

  namespace
  {
    std::string buildTxCore(const Transaction &tx)
    {
      std::ostringstream oss;
      oss << tx.senderPublicKey << '|'
          << tx.receiverPublicKey << '|'
          << tx.fromAddress << '|'
          << tx.toAddress << '|'
          << tx.amount << '|'
          << tx.nonce << '|'
          << tx.payload << '|'
          << tx.timestamp;
      return oss.str();
    }
  }

  void Transaction::computeTxID()
  {
    txID = crypto::sha256(buildTxCore(*this));
  }

  void Transaction::sign(const std::string &privateKey)
  {
    if (txID.empty())
    {
      computeTxID();
    }
    digitalSignature = crypto::sign(txID, privateKey);
  }

  bool Transaction::verify() const
  {
    if (isCoinbase())
    {
      return true; // Coinbase transactions are always valid
    }
    if (amount == 0)
    {
      return false;
    }
    if (txID.empty() || digitalSignature.empty() || senderPublicKey.empty())
    {
      return false;
    }

    std::string expectedFrom = crypto::sha256(senderPublicKey);
    if (!fromAddress.empty() && fromAddress != expectedFrom)
    {
      return false;
    }

    // Recompute txID to ensure it matches
    std::string computedID = crypto::sha256(buildTxCore(*this));
    if (computedID != txID)
    {
      return false;
    }
    return crypto::verify(txID, digitalSignature, senderPublicKey);
  }

  bool Transaction::isExpired(int64_t windowSeconds) const
  {
    // Parse ISO 8601 timestamp
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail())
    {
      return true; // Invalid timestamp = treat as expired
    }

    auto txTime = std::chrono::system_clock::from_time_t(
#ifdef _WIN32
        _mkgmtime(&tm)
#else
        timegm(&tm)
#endif
    );

    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - txTime);

    return std::abs(diff.count()) > windowSeconds;
  }

  std::string Transaction::currentTimestamp()
  {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
  }

  nlohmann::json Transaction::toJson() const
  {
    return nlohmann::json{{"txID", txID},
                          {"senderPublicKey", senderPublicKey},
                          {"receiverPublicKey", receiverPublicKey},
                          {"fromAddress", fromAddress},
                          {"toAddress", toAddress},
                          {"amount", amount},
                          {"nonce", nonce},
                          {"payload", payload},
                          {"timestamp", timestamp},
                          {"digitalSignature", digitalSignature}};
  }

  Transaction Transaction::fromJson(const nlohmann::json &j)
  {
    Transaction tx;
    tx.txID = j.value("txID", "");
    tx.senderPublicKey = j.value("senderPublicKey", "");
    tx.receiverPublicKey = j.value("receiverPublicKey", "");
    tx.fromAddress = j.value("fromAddress", "");
    tx.toAddress = j.value("toAddress", "");
    tx.amount = j.value("amount", uint64_t(0));
    tx.nonce = j.value("nonce", uint64_t(0));
    tx.payload = j.value("payload", "");
    tx.timestamp = j.value("timestamp", "");
    tx.digitalSignature = j.value("digitalSignature", "");

    // Backward compatibility for older chain entries.
    if (tx.fromAddress.empty() && !tx.senderPublicKey.empty() && tx.senderPublicKey != "COINBASE")
    {
      tx.fromAddress = crypto::sha256(tx.senderPublicKey);
    }
    if (tx.toAddress.empty() && !tx.receiverPublicKey.empty())
    {
      tx.toAddress = crypto::sha256(tx.receiverPublicKey);
    }
    if (tx.amount == 0 && tx.isCoinbase())
    {
      tx.amount = 50;
    }
    return tx;
  }

  bool Transaction::isCoinbase() const
  {
    return senderPublicKey == "COINBASE";
  }

} // namespace blockchain
