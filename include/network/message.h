#pragma once

#include "utils/json.hpp"
#include <string>

namespace blockchain
{
  namespace network
  {

    /**
     * Message types for P2P communication.
     */
    enum class MessageType
    {
      NEW_TRANSACTION,    // Broadcast a new transaction
      NEW_BLOCK,          // Broadcast a newly mined block
      REQUEST_CHAIN,      // Request the full chain from a peer
      RESPONSE_CHAIN,     // Respond with the full chain
      PING,               // Keep-alive ping
      PONG,               // Keep-alive pong
      PEER_LIST,          // Share known peers (legacy, kept for compat)
      REQUEST_PEERS,      // Gossip: request peer list from a peer
      RESPONSE_PEERS,     // Gossip: respond with known peer addresses
      NODE_ANNOUNCE,      // Share node identity and public endpoint
      REGISTER_NODE,      // Register with bootstrap node
      REQUEST_BOOTSTRAP,  // Ask bootstrap node for known topology
      RESPONSE_BOOTSTRAP, // Bootstrap response with peer topology
      UNKNOWN
    };

    /**
     * A network message with type and JSON payload.
     */
    struct Message
    {
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
