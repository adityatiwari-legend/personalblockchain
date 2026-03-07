#include "network/node.h"
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

      // Request chain from peers after a short delay
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(2));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
    if (!ec) {
      requestChainFromPeers();
    } });

      // Start gossip-based peer discovery timer
      startGossipTimer();

      // Start heartbeat (PING/PONG) timer
      startHeartbeatTimer();
    }

    void Node::connectToPeer(uint16_t port) { connectToPeer("127.0.0.1", port); }

    void Node::connectToPeer(const std::string &host, uint16_t port, int retryCount)
    {
      // Check if already connected to this peer
      if (isAlreadyConnected(host, port))
      {
        return;
      }

      // Check if peer is banned
      std::string peerKey = host + ":" + std::to_string(port);
      if (peerScorer_.isBanned(peerKey))
      {
        std::cerr << "[Node] Refusing to connect to banned peer " << peerKey << std::endl;
        return;
      }

      // Check max peer limit
      {
        std::lock_guard<std::mutex> lock(peersMutex_);
        if (peers_.size() >= MAX_PEERS)
        {
          std::cout << "[Node] Max peers reached (" << MAX_PEERS << "). Skipping " << peerKey << std::endl;
          return;
        }
      }

      // Register in peer scorer
      peerScorer_.addPeer(host, port);

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

                  std::cout << "[Node] Connected to peer " << host << ":" << port
                            << std::endl;

                  auto peer = std::make_shared<Peer>(
                      std::move(*socket),
                      [this](std::shared_ptr<Peer> p, const Message &msg)
                      {
                        handleMessage(p, msg);
                      });

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

                  // Also request their peer list (gossip pull)
                  Message reqPeers;
                  reqPeers.type = MessageType::REQUEST_PEERS;
                  reqPeers.payload = nlohmann::json::object();
                  peer->send(reqPeers);
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
      int delaySec = std::min(2 << retryCount, 30); // Exponential backoff: 2, 4, 8, 16, 30
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
        // Limit seen set size
        if (seenTxIDs_.size() > 10000)
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
        if (seenBlockHashes_.size() > 1000)
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

    void Node::penalizePeerByEndpoint(const std::string &endpoint, int32_t penalty)
    {
      bool banned = peerScorer_.penalizePeer(endpoint, penalty);
      if (banned)
      {
        // Disconnect the banned peer
        std::lock_guard<std::mutex> lock(peersMutex_);
        for (auto &peer : peers_)
        {
          if (peer->isConnected() && peer->getEndpoint() == endpoint)
          {
            std::cerr << "[Node] Disconnecting banned peer: " << endpoint << std::endl;
            peer->close();
          }
        }
      }
    }

    void Node::rewardPeerByEndpoint(const std::string &endpoint, int32_t reward)
    {
      peerScorer_.rewardPeer(endpoint, reward);
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
              std::string remoteAddr = socket.remote_endpoint().address().to_string();
              uint16_t remotePort = socket.remote_endpoint().port();
              std::string peerKey = remoteAddr + ":" + std::to_string(remotePort);

              std::cout << "[Node] Incoming connection from " << peerKey << std::endl;

              // Check ban status
              if (peerScorer_.isBanned(peerKey))
              {
                std::cerr << "[Node] Rejecting banned peer: " << peerKey << std::endl;
                boost::system::error_code shutdownEc;
                socket.shutdown(tcp::socket::shutdown_both, shutdownEc);
              }
              else
              {
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
            }

            doAccept(); // Continue accepting
          });
    }

    void Node::handleMessage(std::shared_ptr<Peer> peer, const Message &msg)
    {
      // Rate limiting
      if (peer->isRateLimited(50))
      {
        std::cerr << "[Node] Rate limiting peer " << peer->getEndpoint()
                  << std::endl;
        penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_TX);
        return;
      }

      // Mark peer as recently seen
      peerScorer_.markSeen(peer->getEndpoint());

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
            }
          }

          if (!alreadySeen)
          {
            bool accepted = blockchain_.addTransaction(tx);
            if (accepted)
            {
              rewardPeerByEndpoint(peer->getEndpoint(), PeerScorer::REWARD_VALID_TX);
              // Re-broadcast to other peers
              Message fwd;
              fwd.type = MessageType::NEW_TRANSACTION;
              fwd.payload = msg.payload;
              broadcast(fwd, peer);
            }
            else
            {
              penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_TX);
            }
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse transaction: " << e.what()
                    << std::endl;
          penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_TX);
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
            }
          }

          if (!alreadySeen)
          {
            // Validate block before accepting
            if (!block.isValid())
            {
              std::cerr << "[Node] Received invalid block #" << block.index
                        << " from " << peer->getEndpoint() << std::endl;
              penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_BLOCK);
              break;
            }

            // Try to add this block by replacing chain if longer
            auto chain = blockchain_.getChain();
            if (block.index == chain.size() &&
                block.previousHash == chain.back().hash)
            {
              // Append the block
              chain.push_back(block);
              if (blockchain_.replaceChain(chain))
              {
                rewardPeerByEndpoint(peer->getEndpoint(), PeerScorer::REWARD_VALID_BLOCK);
              }
            }
            else if (block.index >= chain.size())
            {
              // We might be behind, request full chain
              Message req;
              req.type = MessageType::REQUEST_CHAIN;
              req.payload = nlohmann::json::object();
              peer->send(req);
            }

            // Re-broadcast
            Message fwd;
            fwd.type = MessageType::NEW_BLOCK;
            fwd.payload = msg.payload;
            broadcast(fwd, peer);
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse block: " << e.what() << std::endl;
          penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_BLOCK);
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
            rewardPeerByEndpoint(peer->getEndpoint(), PeerScorer::REWARD_VALID_BLOCK);
          }
        }
        catch (const std::exception &e)
        {
          std::cerr << "[Node] Failed to parse chain: " << e.what() << std::endl;
          penalizePeerByEndpoint(peer->getEndpoint(), PeerScorer::PENALTY_INVALID_BLOCK);
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
        // Connection is alive — heartbeat acknowledged
        break;

      // ─── Gossip: peer discovery ────────────────────────────────────
      case MessageType::REQUEST_PEERS:
      {
        // Respond with our known non-banned peers
        auto shareablePeers = peerScorer_.getShareablePeers();

        // Also include our currently connected peers
        auto connectedPeers = getPeerList();
        for (const auto &ep : connectedPeers)
        {
          shareablePeers.push_back(ep);
        }

        // Add our own listening address so the requester can share us
        shareablePeers.push_back("self:" + std::to_string(port_));

        Message response;
        response.type = MessageType::RESPONSE_PEERS;
        response.payload = nlohmann::json(shareablePeers);
        peer->send(response);
        break;
      }

      case MessageType::RESPONSE_PEERS:
      {
        handleReceivedPeers(msg.payload);
        break;
      }

      case MessageType::PEER_LIST:
      {
        // Legacy handler — treat as RESPONSE_PEERS
        handleReceivedPeers(msg.payload);
        break;
      }

      default:
        std::cerr << "[Node] Unknown message type from " << peer->getEndpoint()
                  << std::endl;
        break;
      }
    }

    // ─── Gossip implementation ──────────────────────────────────────

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

      for (const auto &peerAddr : payload)
      {
        if (!peerAddr.is_string())
          continue;

        std::string addr = peerAddr.get<std::string>();

        // Skip "self:" entries — these tell us the sender's listen port,
        // but we'd need the sender's IP which we already have from the connection
        if (addr.substr(0, 5) == "self:")
          continue;

        // Parse host:port
        size_t colonPos = addr.rfind(':');
        if (colonPos == std::string::npos || colonPos == 0)
          continue;

        std::string host = addr.substr(0, colonPos);
        uint16_t port = 0;
        try
        {
          port = static_cast<uint16_t>(std::stoi(addr.substr(colonPos + 1)));
        }
        catch (...)
        {
          continue;
        }

        // Don't connect to ourselves
        if (port == port_ && (host == "127.0.0.1" || host == "localhost"))
          continue;

        // Don't connect if already connected or banned
        if (isAlreadyConnected(host, port))
          continue;
        if (peerScorer_.isBanned(host + ":" + std::to_string(port)))
          continue;

        // Connect to the newly discovered peer
        std::cout << "[Gossip] Discovered new peer: " << host << ":" << port << std::endl;
        connectToPeer(host, port);
      }
    }

    void Node::startGossipTimer()
    {
      // Every 30 seconds, request peer lists from all connected peers
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(30));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
    if (!ec) {
      requestPeersFromAll();
      // Also evict stale peers from the scorer
      peerScorer_.evictStalePeers(300);
      startGossipTimer(); // Reschedule
    } });
    }

    void Node::startHeartbeatTimer()
    {
      // Every 15 seconds, ping all connected peers and clean up dead ones
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(15));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
    if (!ec) {
      Message ping;
      ping.type = MessageType::PING;
      ping.payload = nlohmann::json::object();
      broadcast(ping);

      // Clean up disconnected peers
      {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                    [](const std::shared_ptr<Peer> &p) {
                                      return !p->isConnected();
                                    }),
                     peers_.end());
      }

      startHeartbeatTimer(); // Reschedule
    } });
    }

    bool Node::isAlreadyConnected(const std::string &host, uint16_t port) const
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      std::string target = host + ":" + std::to_string(port);
      for (const auto &peer : peers_)
      {
        if (peer->isConnected() && peer->getEndpoint() == target)
        {
          return true;
        }
      }
      return false;
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

  } // namespace network
} // namespace blockchain
