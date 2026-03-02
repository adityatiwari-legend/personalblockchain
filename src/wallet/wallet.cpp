#include "wallet/wallet.h"
#include "crypto/ecdsa.h"
#include "utils/json.hpp"

#include <cctype>
// Suppress OpenSSL 3.0 deprecation warnings for EC_KEY APIs
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <stdexcept>

namespace blockchain {

Wallet::Wallet() {
  auto kp = crypto::generateKeyPair();
  privateKey_ = kp.privateKey;
  publicKey_ = kp.publicKey;
}

Wallet::Wallet(const std::string &privateKeyHex) : privateKey_(privateKeyHex) {
  // Derive public key from private key using OpenSSL
  EC_KEY *ecKey = EC_KEY_new_by_curve_name(NID_secp256k1);
  if (!ecKey)
    throw std::runtime_error("Failed to create EC_KEY");

  BIGNUM *privBN = nullptr;
  if (BN_hex2bn(&privBN, privateKeyHex.c_str()) == 0) {
    EC_KEY_free(ecKey);
    throw std::runtime_error("Failed to parse private key hex");
  }

  if (EC_KEY_set_private_key(ecKey, privBN) != 1) {
    BN_free(privBN);
    EC_KEY_free(ecKey);
    throw std::runtime_error("Failed to set private key");
  }

  const EC_GROUP *group = EC_KEY_get0_group(ecKey);
  EC_POINT *pubPoint = EC_POINT_new(group);
  BN_CTX *ctx = BN_CTX_new();

  if (EC_POINT_mul(group, pubPoint, privBN, nullptr, nullptr, ctx) != 1) {
    EC_POINT_free(pubPoint);
    BN_CTX_free(ctx);
    BN_free(privBN);
    EC_KEY_free(ecKey);
    throw std::runtime_error("Failed to derive public key");
  }

  EC_KEY_set_public_key(ecKey, pubPoint);

  char *pubHex =
      EC_POINT_point2hex(group, pubPoint, POINT_CONVERSION_COMPRESSED, ctx);
  if (!pubHex) {
    EC_POINT_free(pubPoint);
    BN_CTX_free(ctx);
    BN_free(privBN);
    EC_KEY_free(ecKey);
    throw std::runtime_error("Failed to convert public key to hex");
  }

  publicKey_ = std::string(pubHex);
  for (auto &c : publicKey_)
    c = static_cast<char>(std::tolower(c));

  OPENSSL_free(pubHex);
  EC_POINT_free(pubPoint);
  BN_CTX_free(ctx);
  BN_free(privBN);
  EC_KEY_free(ecKey);
}

std::string Wallet::signData(const std::string &data) const {
  return crypto::sign(data, privateKey_);
}

std::string Wallet::toJson() const {
  nlohmann::json j;
  j["publicKey"] = publicKey_;
  return j.dump();
}

} // namespace blockchain
