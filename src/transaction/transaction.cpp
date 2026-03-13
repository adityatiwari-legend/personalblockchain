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

  void Transaction::computeTxID()
  {
    std::string raw = senderPublicKey + receiverPublicKey + payload + timestamp;
    txID = crypto::sha256(raw);
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
    if (txID.empty() || digitalSignature.empty() || senderPublicKey.empty())
    {
      return false;
    }
    // Recompute txID to ensure it matches
    std::string raw = senderPublicKey + receiverPublicKey + payload + timestamp;
    std::string computedID = crypto::sha256(raw);
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
    tx.payload = j.value("payload", "");
    tx.timestamp = j.value("timestamp", "");
    tx.digitalSignature = j.value("digitalSignature", "");
    return tx;
  }

  bool Transaction::isCoinbase() const
  {
    return senderPublicKey == "COINBASE";
  }

} // namespace blockchain
