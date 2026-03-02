#pragma once

#include "utils/json.hpp"
#include <string>

namespace blockchain {
namespace network {

/**
 * Message types for P2P communication.
 */
enum class MessageType {
  NEW_TRANSACTION, // Broadcast a new transaction
  NEW_BLOCK,       // Broadcast a newly mined block
  REQUEST_CHAIN,   // Request the full chain from a peer
  RESPONSE_CHAIN,  // Respond with the full chain
  PING,            // Keep-alive ping
  PONG,            // Keep-alive pong
  PEER_LIST,       // Share known peers
  UNKNOWN
};

/**
 * A network message with type and JSON payload.
 */
struct Message {
  MessageType type;
  nlohmann::json payload;

  /** Serialize to a length-prefixed string for network transmission. */
  std::string serialize() const;

  /** Deserialize from raw data. */
  static Message deserialize(const std::string &data);

  /** Convert MessageType to string. */
  static std::string typeToString(MessageType type);

  /** Convert string to MessageType. */
  static MessageType stringToType(const std::string &str);
};

} // namespace network
} // namespace blockchain
