#include "network/peer.h"
#include <algorithm>
#include <iostream>

namespace blockchain
{
  namespace network
  {

    Peer::Peer(tcp::socket socket, MessageHandler handler)
        : socket_(std::move(socket)), messageHandler_(std::move(handler)) {}

    void Peer::start() { doReadHeader(); }

    void Peer::send(const Message &msg)
    {
      std::string data = msg.serialize();

      std::lock_guard<std::mutex> lock(writeMutex_);

      // Drop messages if write queue is too large (slow/malicious peer)
      if (writeQueue_.size() >= MAX_WRITE_QUEUE)
      {
        std::cerr << "[Peer] Write queue full for " << getEndpoint()
                  << ", dropping message" << std::endl;
        return;
      }

      writeQueue_.push_back(std::move(data));

      if (!writing_)
      {
        writing_ = true;
        doWrite();
      }
    }

    void Peer::close()
    {
      boost::system::error_code ec;
      socket_.shutdown(tcp::socket::shutdown_both, ec);
      socket_.close(ec);
    }

    bool Peer::isConnected() const { return socket_.is_open(); }

    std::string Peer::getEndpoint() const
    {
      try
      {
        auto ep = socket_.remote_endpoint();
        // Use the known listen port if available (fixes ephemeral port identity issue)
        uint16_t port = (knownListenPort_ > 0) ? knownListenPort_ : ep.port();
        return ep.address().to_string() + ":" + std::to_string(port);
      }
      catch (...)
      {
        return "unknown";
      }
    }

    uint16_t Peer::getPort() const
    {
      try
      {
        return socket_.remote_endpoint().port();
      }
      catch (...)
      {
        return 0;
      }
    }

    bool Peer::isRateLimited(int maxPerSecond)
    {
      std::lock_guard<std::mutex> lock(rateMutex_);
      auto now = std::chrono::steady_clock::now();
      messageTimes_.push_back(now);

      // Remove entries older than 1 second
      auto cutoff = now - std::chrono::seconds(1);
      while (!messageTimes_.empty() && messageTimes_.front() < cutoff)
      {
        messageTimes_.pop_front();
      }

      return static_cast<int>(messageTimes_.size()) > maxPerSecond;
    }

    void Peer::doReadHeader()
    {
      auto self = shared_from_this();
      boost::asio::async_read(
          socket_, boost::asio::buffer(headerBuf_),
          [this, self](boost::system::error_code ec, std::size_t /*length*/)
          {
            if (ec)
            {
              if (ec != boost::asio::error::eof &&
                  ec != boost::asio::error::operation_aborted)
              {
                std::cerr << "[Peer] Read header error from " << getEndpoint()
                          << ": " << ec.message() << std::endl;
              }
              close();
              return;
            }

            // Parse length (big-endian)
            uint32_t bodyLen = 0;
            bodyLen |=
                static_cast<uint32_t>(static_cast<unsigned char>(headerBuf_[0]))
                << 24;
            bodyLen |=
                static_cast<uint32_t>(static_cast<unsigned char>(headerBuf_[1]))
                << 16;
            bodyLen |=
                static_cast<uint32_t>(static_cast<unsigned char>(headerBuf_[2]))
                << 8;
            bodyLen |=
                static_cast<uint32_t>(static_cast<unsigned char>(headerBuf_[3]));

            // Reject messages larger than 10 MB
            if (bodyLen > 10 * 1024 * 1024)
            {
              std::cerr << "[Peer] Rejected oversized message from "
                        << getEndpoint() << std::endl;
              close();
              return;
            }

            doReadBody(bodyLen);
          });
    }

    void Peer::doReadBody(uint32_t length)
    {
      bodyBuf_.resize(length);
      auto self = shared_from_this();

      boost::asio::async_read(
          socket_, boost::asio::buffer(bodyBuf_),
          [this, self](boost::system::error_code ec, std::size_t /*length*/)
          {
            if (ec)
            {
              if (ec != boost::asio::error::eof &&
                  ec != boost::asio::error::operation_aborted)
              {
                std::cerr << "[Peer] Read body error from " << getEndpoint() << ": "
                          << ec.message() << std::endl;
              }
              close();
              return;
            }

            std::string data(bodyBuf_.begin(), bodyBuf_.end());
            Message msg = Message::deserialize(data);

            if (msg.type == MessageType::UNKNOWN)
            {
              std::cerr << "[Peer] Malformed message from " << getEndpoint()
                        << std::endl;
            }
            else
            {
              if (messageHandler_)
              {
                messageHandler_(shared_from_this(), msg);
              }
            }

            // Continue reading
            doReadHeader();
          });
    }

    void Peer::doWrite()
    {
      auto self = shared_from_this();
      boost::asio::async_write(
          socket_, boost::asio::buffer(writeQueue_.front()),
          [this, self](boost::system::error_code ec, std::size_t /*length*/)
          {
            std::lock_guard<std::mutex> lock(writeMutex_);

            if (ec)
            {
              std::cerr << "[Peer] Write error to " << getEndpoint() << ": "
                        << ec.message() << std::endl;
              writing_ = false;
              writeQueue_.clear();
              return;
            }

            writeQueue_.pop_front();
            if (!writeQueue_.empty())
            {
              doWrite();
            }
            else
            {
              writing_ = false;
            }
          });
    }

  } // namespace network
} // namespace blockchain
