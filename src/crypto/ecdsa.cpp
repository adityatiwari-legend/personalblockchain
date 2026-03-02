#include "crypto/ecdsa.h"
#include "crypto/sha256.h"

// Suppress OpenSSL 3.0 deprecation warnings for EC_KEY/ECDSA APIs
#define OPENSSL_SUPPRESS_DEPRECATED

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace blockchain {
namespace crypto {

// --- Hex Utilities ---

std::vector<unsigned char> hexToBytes(const std::string &hex) {
  std::vector<unsigned char> bytes;
  if (hex.size() % 2 != 0) {
    throw std::invalid_argument("Hex string must have even length");
  }
  bytes.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    unsigned int byte;
    std::stringstream ss;
    ss << std::hex << hex.substr(i, 2);
    ss >> byte;
    bytes.push_back(static_cast<unsigned char>(byte));
  }
  return bytes;
}

std::string bytesToHex(const std::vector<unsigned char> &bytes) {
  std::stringstream ss;
  for (auto b : bytes) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
  }
  return ss.str();
}

// --- RAII Helpers ---

struct EC_KEY_Deleter {
  void operator()(EC_KEY *key) {
    if (key)
      EC_KEY_free(key);
  }
};
using EC_KEY_ptr = std::unique_ptr<EC_KEY, EC_KEY_Deleter>;

struct BN_Deleter {
  void operator()(BIGNUM *bn) {
    if (bn)
      BN_free(bn);
  }
};
using BN_ptr = std::unique_ptr<BIGNUM, BN_Deleter>;

struct BN_CTX_Deleter {
  void operator()(BN_CTX *ctx) {
    if (ctx)
      BN_CTX_free(ctx);
  }
};
using BN_CTX_ptr = std::unique_ptr<BN_CTX, BN_CTX_Deleter>;

struct ECDSA_SIG_Deleter {
  void operator()(ECDSA_SIG *sig) {
    if (sig)
      ECDSA_SIG_free(sig);
  }
};
using ECDSA_SIG_ptr = std::unique_ptr<ECDSA_SIG, ECDSA_SIG_Deleter>;

// --- Key Generation ---

KeyPair generateKeyPair() {
  EC_KEY_ptr ecKey(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ecKey) {
    throw std::runtime_error("Failed to create EC_KEY for secp256k1");
  }

  if (EC_KEY_generate_key(ecKey.get()) != 1) {
    throw std::runtime_error("Failed to generate EC key pair");
  }

  KeyPair kp;

  // Extract private key as hex
  const BIGNUM *privBN = EC_KEY_get0_private_key(ecKey.get());
  if (!privBN) {
    throw std::runtime_error("Failed to get private key");
  }
  char *privHex = BN_bn2hex(privBN);
  kp.privateKey = std::string(privHex);
  OPENSSL_free(privHex);
  // Lowercase
  for (auto &c : kp.privateKey)
    c = static_cast<char>(std::tolower(c));

  // Extract public key as compressed hex
  const EC_POINT *pubPoint = EC_KEY_get0_public_key(ecKey.get());
  const EC_GROUP *group = EC_KEY_get0_group(ecKey.get());
  BN_CTX_ptr bnCtx(BN_CTX_new());

  char *pubHex = EC_POINT_point2hex(group, pubPoint,
                                    POINT_CONVERSION_COMPRESSED, bnCtx.get());
  if (!pubHex) {
    throw std::runtime_error("Failed to convert public key to hex");
  }
  kp.publicKey = std::string(pubHex);
  OPENSSL_free(pubHex);
  for (auto &c : kp.publicKey)
    c = static_cast<char>(std::tolower(c));

  return kp;
}

// --- Signing ---

std::string sign(const std::string &message, const std::string &privateKeyHex) {
  // Hash the message first
  std::string hashHex = sha256(message);
  auto hashBytes = hexToBytes(hashHex);

  // Reconstruct the EC_KEY from private key hex
  EC_KEY_ptr ecKey(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ecKey) {
    throw std::runtime_error("Failed to create EC_KEY");
  }

  BIGNUM *privBN = nullptr;
  if (BN_hex2bn(&privBN, privateKeyHex.c_str()) == 0) {
    throw std::runtime_error("Failed to parse private key hex");
  }
  BN_ptr privBNPtr(privBN);

  if (EC_KEY_set_private_key(ecKey.get(), privBN) != 1) {
    throw std::runtime_error("Failed to set private key");
  }

  // Derive public key from private key
  const EC_GROUP *group = EC_KEY_get0_group(ecKey.get());
  EC_POINT *pubPoint = EC_POINT_new(group);
  BN_CTX_ptr ctx(BN_CTX_new());

  if (EC_POINT_mul(group, pubPoint, privBN, nullptr, nullptr, ctx.get()) != 1) {
    EC_POINT_free(pubPoint);
    throw std::runtime_error("Failed to derive public key");
  }

  if (EC_KEY_set_public_key(ecKey.get(), pubPoint) != 1) {
    EC_POINT_free(pubPoint);
    throw std::runtime_error("Failed to set public key");
  }
  EC_POINT_free(pubPoint);

  // Sign the hash
  ECDSA_SIG *sig = ECDSA_do_sign(
      hashBytes.data(), static_cast<int>(hashBytes.size()), ecKey.get());
  if (!sig) {
    throw std::runtime_error("Failed to create ECDSA signature");
  }
  ECDSA_SIG_ptr sigPtr(sig);

  // DER encode the signature
  int derLen = i2d_ECDSA_SIG(sig, nullptr);
  if (derLen <= 0) {
    throw std::runtime_error("Failed to get DER length");
  }

  std::vector<unsigned char> derSig(static_cast<size_t>(derLen));
  unsigned char *derPtr = derSig.data();
  if (i2d_ECDSA_SIG(sig, &derPtr) != derLen) {
    throw std::runtime_error("Failed to DER encode signature");
  }

  return bytesToHex(derSig);
}

// --- Verification ---

bool verify(const std::string &message, const std::string &signatureHex,
            const std::string &publicKeyHex) {
  try {
    // Hash the message
    std::string hashHex = sha256(message);
    auto hashBytes = hexToBytes(hashHex);

    // Decode signature
    auto sigBytes = hexToBytes(signatureHex);
    const unsigned char *sigPtr = sigBytes.data();
    ECDSA_SIG *sig =
        d2i_ECDSA_SIG(nullptr, &sigPtr, static_cast<long>(sigBytes.size()));
    if (!sig) {
      return false;
    }
    ECDSA_SIG_ptr sigRAII(sig);

    // Reconstruct EC_KEY from public key
    EC_KEY_ptr ecKey(EC_KEY_new_by_curve_name(NID_secp256k1));
    if (!ecKey) {
      return false;
    }

    const EC_GROUP *group = EC_KEY_get0_group(ecKey.get());
    EC_POINT *pubPoint = EC_POINT_new(group);
    BN_CTX_ptr ctx(BN_CTX_new());

    if (EC_POINT_hex2point(group, publicKeyHex.c_str(), pubPoint, ctx.get()) ==
        nullptr) {
      EC_POINT_free(pubPoint);
      return false;
    }

    if (EC_KEY_set_public_key(ecKey.get(), pubPoint) != 1) {
      EC_POINT_free(pubPoint);
      return false;
    }
    EC_POINT_free(pubPoint);

    // Verify signature
    int result = ECDSA_do_verify(
        hashBytes.data(), static_cast<int>(hashBytes.size()), sig, ecKey.get());
    return result == 1;
  } catch (...) {
    return false;
  }
}

} // namespace crypto
} // namespace blockchain
