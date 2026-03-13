#include "network/message.h"
#include <stdexcept>

namespace blockchain
{
  namespace network
  {

    std::string Message::serialize() const
    {
      nlohmann::json j;
      j["type"] = typeToString(type);
      j["payload"] = payload;

      std::string data = j.dump();

      // Length-prefix: 4 bytes (network byte order)
      uint32_t len = static_cast<uint32_t>(data.size());
      std::string result(4, '\0');
      result[0] = static_cast<char>((len >> 24) & 0xFF);
      result[1] = static_cast<char>((len >> 16) & 0xFF);
      result[2] = static_cast<char>((len >> 8) & 0xFF);
      result[3] = static_cast<char>(len & 0xFF);
      result += data;

      return result;
    }

    Message Message::deserialize(const std::string &data)
    {
      Message msg;
      try
      {
        nlohmann::json j = nlohmann::json::parse(data);
        msg.type = stringToType(j.value("type", "UNKNOWN"));
        msg.payload = j.value("payload", nlohmann::json::object());
      }
      catch (const std::exception &)
      {
        msg.type = MessageType::UNKNOWN;
        msg.payload = nlohmann::json::object();
      }
      return msg;
    }

    std::string Message::typeToString(MessageType type)
    {
      switch (type)
      {
      case MessageType::NEW_TRANSACTION:
        return "NEW_TRANSACTION";
      case MessageType::NEW_BLOCK:
        return "NEW_BLOCK";
      case MessageType::REQUEST_CHAIN:
        return "REQUEST_CHAIN";
      case MessageType::RESPONSE_CHAIN:
        return "RESPONSE_CHAIN";
      case MessageType::PING:
        return "PING";
      case MessageType::PONG:
        return "PONG";
      case MessageType::PEER_LIST:
        return "PEER_LIST";
      case MessageType::REQUEST_PEERS:
        return "REQUEST_PEERS";
      case MessageType::RESPONSE_PEERS:
        return "RESPONSE_PEERS";
      default:
        return "UNKNOWN";
      }
    }

    MessageType Message::stringToType(const std::string &str)
    {
      if (str == "NEW_TRANSACTION")
        return MessageType::NEW_TRANSACTION;
      if (str == "NEW_BLOCK")
        return MessageType::NEW_BLOCK;
      if (str == "REQUEST_CHAIN")
        return MessageType::REQUEST_CHAIN;
      if (str == "RESPONSE_CHAIN")
        return MessageType::RESPONSE_CHAIN;
      if (str == "PING")
        return MessageType::PING;
      if (str == "PONG")
        return MessageType::PONG;
      if (str == "PEER_LIST")
        return MessageType::PEER_LIST;
      if (str == "REQUEST_PEERS")
        return MessageType::REQUEST_PEERS;
      if (str == "RESPONSE_PEERS")
        return MessageType::RESPONSE_PEERS;
      return MessageType::UNKNOWN;
    }

  } // namespace network
} // namespace blockchain
