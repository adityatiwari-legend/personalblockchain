#pragma once

#include "network/message.h"

#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace blockchain
{
  namespace network
  {

    using boost::asio::ip::tcp;

    /**
     * Represents a connection to a peer node.
     * Handles async read/write with length-prefix framing.
     */
    class Peer : public std::enable_shared_from_this<Peer>
    {
    public:
      using MessageHandler =
          std::function<void(std::shared_ptr<Peer>, const Message &)>;

      Peer(tcp::socket socket, MessageHandler handler);

      /** Start reading messages from this peer. */
      void start();

      /** Send a message to this peer. */
      void send(const Message &msg);

      /** Close the connection. */
      void close();

      /** Check if the connection is open. */
      bool isConnected() const;

      /** Get the remote endpoint as a string (uses listen port if known). */
      std::string getEndpoint() const;

      /** Get the port of the remote endpoint. */
      uint16_t getPort() const;

      /** Set the known listen port for this peer (for scoring identity). */
      void setListenPort(uint16_t port) { knownListenPort_ = port; }

      /** Get the known listen port (0 if unknown). */
      uint16_t getListenPort() const { return knownListenPort_; }

      /**
       * Rate limiting: check if this peer is sending too many messages.
       * @param maxPerSecond Maximum messages allowed per second
       * @return true if peer should be rate-limited (too many messages)
       */
      bool isRateLimited(int maxPerSecond = 50);

    private:
      void doReadHeader();
      void doReadBody(uint32_t length);
      void doWrite();

      tcp::socket socket_;
      MessageHandler messageHandler_;

      // Read buffer
      std::array<char, 4> headerBuf_;
      std::vector<char> bodyBuf_;

      // Write queue
      std::mutex writeMutex_;
      std::deque<std::string> writeQueue_;
      bool writing_ = false;
      static constexpr size_t MAX_WRITE_QUEUE = 1000;

      // Known listen port for peer identity (0 = unknown, use ephemeral)
      uint16_t knownListenPort_ = 0;

      // Rate limiting
      std::deque<std::chrono::steady_clock::time_point> messageTimes_;
      std::mutex rateMutex_;
    };

  } // namespace network
} // namespace blockchain
