#include "network/node.h"
#include <algorithm>
#include <iostream>

namespace blockchain
{
  namespace network
  {

    Node::Node(boost::asio::io_context &ioContext, Blockchain &blockchain,
               uint16_t port)
        : ioContext_(ioContext),
          acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)),
          blockchain_(blockchain), port_(port)
    {

      // Set up blockchain callbacks for auto-broadcasting
      blockchain_.setOnBlockAdded(
          [this](const Block &block)
          { broadcastBlock(block); });

      blockchain_.setOnTransactionAdded(
          [this](const Transaction &tx)
          { broadcastTransaction(tx); });

      std::cout << "[Node] Listening on port " << port << std::endl;
    }

    void Node::start()
    {
      doAccept();

      // Start heartbeat and gossip timers
      startHeartbeatTimer();
      startGossipTimer();

      // Request chain from peers after a short delay
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(2));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
    if (!ec) {
      requestChainFromPeers();
    } });
    }

    void Node::connectToPeer(uint16_t port) { connectToPeer("127.0.0.1", port); }

    void Node::connectToPeer(const std::string &host, uint16_t port, int retryCount)
    {
      // Check if already connected
      if (isAlreadyConnected(host, port))
      {
        return;
      }

      auto resolver = std::make_shared<tcp::resolver>(ioContext_);
      resolver->async_resolve(
          host, std::to_string(port),
          [this, resolver, host, port, retryCount](boost::system::error_code ec,
                                                   tcp::resolver::results_type results)
          {
            if (ec)
            {
              std::cerr << "[Node] Failed to resolve " << host << ":" << port
                        << ": " << ec.message() << std::endl;
              retryConnect(host, port, retryCount);
              return;
            }

            auto socket = std::make_shared<tcp::socket>(ioContext_);
            boost::asio::async_connect(
                *socket, results,
                [this, socket, host, port, retryCount](boost::system::error_code ec,
                                                       const tcp::endpoint & /*endpoint*/)
                {
                  if (ec)
                  {
                    std::cerr << "[Node] Failed to connect to " << host << ":"
                              << port << ": " << ec.message() << std::endl;
                    retryConnect(host, port, retryCount);
                    return;
                  }

                  // Enforce MAX_PEERS
                  {
                    std::lock_guard<std::mutex> lock(peersMutex_);
                    if (peers_.size() >= MAX_PEERS)
                    {
                      std::cerr << "[Node] MAX_PEERS reached, dropping new connection to "
                                << host << ":" << port << std::endl;
                      boost::system::error_code shutEc;
                      socket->shutdown(tcp::socket::shutdown_both, shutEc);
                      return;
                    }
                  }

                  std::cout << "[Node] Connected to peer " << host << ":" << port
                            << std::endl;

                  auto peer = std::make_shared<Peer>(
                      std::move(*socket),
                      [this](std::shared_ptr<Peer> p, const Message &msg)
                      {
                        handleMessage(p, msg);
                      });

                  peer->setListenPort(port);

                  // Register peer in scorer
                  peerScorer_.addPeer(host, port);

                  {
                    std::lock_guard<std::mutex> lock(peersMutex_);
                    peers_.push_back(peer);
                  }

                  peer->start();

                  // Request chain from this new peer
                  Message reqChain;
                  reqChain.type = MessageType::REQUEST_CHAIN;
                  reqChain.payload = nlohmann::json::object();
                  peer->send(reqChain);
                });
          });
    }

    void Node::retryConnect(const std::string &host, uint16_t port, int retryCount)
    {
      static const int MAX_RETRIES = 5;
      if (retryCount >= MAX_RETRIES)
      {
        std::cerr << "[Node] Max retries reached for " << host << ":" << port
                  << ". Giving up." << std::endl;
        return;
      }
      int delaySec = std::min(2 << retryCount, 30);
      std::cout << "[Node] Retrying connection to " << host << ":" << port
                << " in " << delaySec << "s (attempt " << (retryCount + 1)
                << "/" << MAX_RETRIES << ")" << std::endl;

      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(delaySec));
      timer->async_wait([this, timer, host, port, retryCount](boost::system::error_code ec)
                        {
    if (!ec) {
      connectToPeer(host, port, retryCount + 1);
    } });
    }

    void Node::broadcastTransaction(const Transaction &tx)
    {
      {
        std::lock_guard<std::mutex> lock(seenMutex_);
        if (seenTxIDs_.count(tx.txID))
          return;
        seenTxIDs_.insert(tx.txID);
        while (seenTxIDs_.size() > 10000)
        {
          seenTxIDs_.erase(seenTxIDs_.begin());
        }
      }

      Message msg;
      msg.type = MessageType::NEW_TRANSACTION;
      msg.payload = tx.toJson();
      broadcast(msg);
    }

    void Node::broadcastBlock(const Block &block)
    {
      {
        std::lock_guard<std::mutex> lock(seenMutex_);
        if (seenBlockHashes_.count(block.hash))
          return;
        seenBlockHashes_.insert(block.hash);
        while (seenBlockHashes_.size() > 1000)
        {
          seenBlockHashes_.erase(seenBlockHashes_.begin());
        }
      }

      Message msg;
      msg.type = MessageType::NEW_BLOCK;
      msg.payload = block.toJson();
      broadcast(msg);
    }

    void Node::requestChainFromPeers()
    {
      Message msg;
      msg.type = MessageType::REQUEST_CHAIN;
      msg.payload = nlohmann::json::object();
      broadcast(msg);
    }

    std::vector<std::string> Node::getPeerList() const
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      std::vector<std::string> list;
      for (const auto &peer : peers_)
      {
        if (peer->isConnected())
        {
          list.push_back(peer->getEndpoint());
        }
      }
      return list;
    }

    void Node::doAccept()
    {
      acceptor_.async_accept(
          [this](boost::system::error_code ec, tcp::socket socket)
          {
            if (ec)
            {
              std::cerr << "[Node] Accept error: " << ec.message() << std::endl;
            }
            else
            {
              // Enforce MAX_PEERS limit
              {
                std::lock_guard<std::mutex> lock(peersMutex_);
                if (peers_.size() >= MAX_PEERS)
                {
                  std::cerr << "[Node] MAX_PEERS reached, rejecting incoming connection" << std::endl;
                  boost::system::error_code shutEc;
                  socket.shutdown(tcp::socket::shutdown_both, shutEc);
                  doAccept();
                  return;
                }
              }

              std::cout << "[Node] Incoming connection from "
                        << socket.remote_endpoint().address().to_string() << ":"
                        << socket.remote_endpoint().port() << std::endl;

              auto peer = std::make_shared<Peer>(
                  std::move(socket),
                  [this](std::shared_ptr<Peer> p, const Message &msg)
                  {
                    handleMessage(p, msg);
                  });

              {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_.push_back(peer);
              }

              peer->start();
            }

            doAccept();
          });
    }

    void Node::handleMessage(std::shared_ptr<Peer> peer, const Message &msg)
    {
      const std::string peerKey = peer->getEndpoint();
      const auto peerPort = peer->getListenPort() > 0 ? peer->getListenPort() : peer->getPort();
      const size_t sep = peerKey.rfind(':');
      if (sep != std::string::npos && sep > 0)
      {
        peerScorer_.addPeer(peerKey.substr(0, sep), peerPort);
      }

      // Rate limiting
      if (peer->isRateLimited(50))
      {
        std::cerr << "[Node] Rate limiting peer " << peer->getEndpoint()
                  << std::endl;
        return;
      }

      // Check ban status
      if (peerScorer_.isBanned(peerKey))
      {
        std::cerr << "[Node] Ignoring message from banned peer " << peerKey << std::endl;
        return;
      }

      // Mark peer as seen
      peerScorer_.markSeen(peerKey);

      switch (msg.type)
      {
      case MessageType::NEW_TRANSACTION:
      {
        try
        {
          Transaction tx = Transaction::fromJson(msg.payload);
          bool alreadySeen = false;
          {
            std::lock_guard<std::mutex> lock(seenMutex_);
            alreadySeen = seenTxIDs_.count(tx.txID) > 0;
            if (!alreadySeen)
            {
              seenTxIDs_.insert(tx.txID);
              while (seenTxIDs_.size() > 10000)
              {
                seenTxIDs_.erase(seenTxIDs_.begin());
              }
            }
          }

          if (!alreadySeen)
          {
            bool accepted = blockchain_.addTransaction(tx);
            if (accepted)
            {
              rewardPeerByEndpoint(peerKey, PeerScorer::REWARD_VALID_TX);
              Message fwd;
              fwd.type = MessageType::NEW_TRANSACTION;
              fwd.payload = msg.payload;
              broadcast(fwd, peer);
            }
            else
            {
              penalizePeerByEndpoint(peerKey, PeerScorer::PENALTY_INVALID_TX);
            }
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse transaction: " << e.what()
                    << std::endl;
          penalizePeerByEndpoint(peerKey, PeerScorer::PENALTY_INVALID_TX);
        }
        break;
      }

      case MessageType::NEW_BLOCK:
      {
        try
        {
          Block block = Block::fromJson(msg.payload);
          bool alreadySeen = false;
          {
            std::lock_guard<std::mutex> lock(seenMutex_);
            alreadySeen = seenBlockHashes_.count(block.hash) > 0;
            if (!alreadySeen)
            {
              seenBlockHashes_.insert(block.hash);
              while (seenBlockHashes_.size() > 1000)
              {
                seenBlockHashes_.erase(seenBlockHashes_.begin());
              }
            }
          }

          if (!alreadySeen)
          {
            auto chain = blockchain_.getChain();
            bool accepted = false;

            if (block.index == chain.size() &&
                block.previousHash == chain.back().hash)
            {
              chain.push_back(block);
              accepted = blockchain_.replaceChain(chain);
            }
            else if (block.index >= chain.size())
            {
              Message req;
              req.type = MessageType::REQUEST_CHAIN;
              req.payload = nlohmann::json::object();
              peer->send(req);
            }

            // Only re-broadcast AFTER validation succeeds
            if (accepted)
            {
              rewardPeerByEndpoint(peerKey, PeerScorer::REWARD_VALID_BLOCK);
              Message fwd;
              fwd.type = MessageType::NEW_BLOCK;
              fwd.payload = msg.payload;
              broadcast(fwd, peer);
            }
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse block: " << e.what() << std::endl;
          penalizePeerByEndpoint(peerKey, PeerScorer::PENALTY_INVALID_BLOCK);
        }
        break;
      }

      case MessageType::REQUEST_CHAIN:
      {
        Message response;
        response.type = MessageType::RESPONSE_CHAIN;
        response.payload = blockchain_.chainToJson();
        peer->send(response);
        break;
      }

      case MessageType::RESPONSE_CHAIN:
      {
        try
        {
          std::vector<Block> newChain;
          for (const auto &blockJson : msg.payload)
          {
            newChain.push_back(Block::fromJson(blockJson));
          }
          if (blockchain_.replaceChain(newChain))
          {
            rewardPeerByEndpoint(peerKey, PeerScorer::REWARD_VALID_BLOCK);
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse chain: " << e.what() << std::endl;
          penalizePeerByEndpoint(peerKey, PeerScorer::PENALTY_INVALID_BLOCK);
        }
        break;
      }

      case MessageType::PING:
      {
        Message pong;
        pong.type = MessageType::PONG;
        pong.payload = nlohmann::json::object();
        peer->send(pong);
        break;
      }

      case MessageType::PONG:
        peerScorer_.markPongReceived(peerKey);
        break;

      case MessageType::REQUEST_PEERS:
      {
        auto shareablePeers = peerScorer_.getShareablePeers();
        nlohmann::json peerArray = nlohmann::json::array();
        for (const auto &p : shareablePeers)
        {
          peerArray.push_back(p);
        }
        Message resp;
        resp.type = MessageType::RESPONSE_PEERS;
        resp.payload = peerArray;
        peer->send(resp);
        break;
      }

      case MessageType::RESPONSE_PEERS:
      {
        handleReceivedPeers(msg.payload);
        break;
      }

      case MessageType::PEER_LIST:
      {
        handleReceivedPeers(msg.payload);
        break;
      }

      default:
        std::cerr << "[Node] Unknown message type from " << peer->getEndpoint()
                  << std::endl;
        break;
      }
    }

    void Node::removePeer(std::shared_ptr<Peer> peer)
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                  [&peer](const std::shared_ptr<Peer> &p)
                                  {
                                    return p == peer || !p->isConnected();
                                  }),
                   peers_.end());
    }

    void Node::broadcast(const Message &msg, std::shared_ptr<Peer> exclude)
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      for (auto &peer : peers_)
      {
        if (peer != exclude && peer->isConnected())
        {
          peer->send(msg);
        }
      }
    }

    // ─── Gossip-based peer discovery ────────────────────────────────────

    void Node::requestPeersFromAll()
    {
      Message msg;
      msg.type = MessageType::REQUEST_PEERS;
      msg.payload = nlohmann::json::object();
      broadcast(msg);
    }

    void Node::handleReceivedPeers(const nlohmann::json &payload)
    {
      if (!payload.is_array())
        return;

      for (const auto &entry : payload)
      {
        if (!entry.is_string())
          continue;

        std::string peerAddr = entry.get<std::string>();
        size_t colonPos = peerAddr.rfind(':');
        if (colonPos == std::string::npos || colonPos == 0)
          continue;

        std::string host = peerAddr.substr(0, colonPos);
        uint16_t port = 0;
        try
        {
          port = static_cast<uint16_t>(std::stoi(peerAddr.substr(colonPos + 1)));
        }
        catch (...)
        {
          continue;
        }

        if (port == port_)
          continue;
        if (isAlreadyConnected(host, port))
          continue;
        if (peerScorer_.isBanned(host + ":" + std::to_string(port)))
          continue;

        std::cout << "[Gossip] Discovered new peer: " << host << ":" << port << std::endl;
        connectToPeer(host, port);
      }
    }

    void Node::startGossipTimer()
    {
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(30));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
        if (!ec)
        {
          requestPeersFromAll();
          peerScorer_.evictStalePeers(300);
          startGossipTimer();
        } });
    }

    // ─── Heartbeat / PING-PONG ──────────────────────────────────────────

    void Node::startHeartbeatTimer()
    {
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(15));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
        if (!ec)
        {
          Message ping;
          ping.type = MessageType::PING;
          ping.payload = nlohmann::json::object();

          {
            std::lock_guard<std::mutex> lock(peersMutex_);
            // Clean up disconnected peers
            peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                        [](const std::shared_ptr<Peer> &p)
                                        {
                                          return !p->isConnected();
                                        }),
                         peers_.end());

            for (auto &peer : peers_)
            {
              peerScorer_.markPingSent(peer->getEndpoint());
              peer->send(ping);
            }
          }

          auto timedOutPeers = peerScorer_.getHeartbeatTimeoutPeers(45);
          if (!timedOutPeers.empty())
          {
            std::lock_guard<std::mutex> lock(peersMutex_);
            for (const auto &endpoint : timedOutPeers)
            {
              for (auto &peer : peers_)
              {
                if (peer->getEndpoint() == endpoint)
                {
                  std::cerr << "[Heartbeat] Disconnecting timed-out peer: "
                            << endpoint << std::endl;
                  peer->close();
                }
              }
            }
          }

          startHeartbeatTimer();
        } });
    }

    // ─── Peer dedup ─────────────────────────────────────────────────────

    bool Node::isAlreadyConnected(const std::string &host, uint16_t port) const
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      for (const auto &peer : peers_)
      {
        if (!peer->isConnected())
          continue;
        if (peer->getListenPort() == port)
          return true;
        if (peer->getPort() == port)
          return true;
      }
      return false;
    }

    // ─── Peer scoring bridge ────────────────────────────────────────────

    void Node::penalizePeerByEndpoint(const std::string &endpoint, int32_t penalty)
    {
      bool banned = peerScorer_.penalizePeer(endpoint, penalty);
      if (banned)
      {
        std::lock_guard<std::mutex> lock(peersMutex_);
        for (auto &peer : peers_)
        {
          if (peer->getEndpoint() == endpoint)
          {
            std::cerr << "[Node] Disconnecting banned peer: " << endpoint << std::endl;
            peer->close();
            break;
          }
        }
      }
    }

    void Node::rewardPeerByEndpoint(const std::string &endpoint, int32_t reward)
    {
      peerScorer_.rewardPeer(endpoint, reward);
    }

  } // namespace network
} // namespace blockchain
