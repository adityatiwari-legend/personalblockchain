#include "network/node.h"

#include "crypto/sha256.h"

#include <algorithm>
#include <iostream>
#include <random>

namespace blockchain
{
  namespace network
  {

    Node::Node(boost::asio::io_context &ioContext, Blockchain &blockchain,
               uint16_t port, const NodeOptions &options)
        : ioContext_(ioContext),
          acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)),
          blockchain_(blockchain),
          port_(port),
          options_(options),
          nodeIdentity_(options.dataDir),
          connectionManager_(options.maxOutboundPeers, options.maxInboundPeers)
    {
      nodeIdentity_.loadOrCreate(port_, options_.publicIp);

      for (const auto &bootstrap : options_.bootstrapNodes)
      {
        peerManager_.addKnownPeer(bootstrap);
      }
      bootstrapManager_.setBootstrapPeers(options_.bootstrapNodes);

      blockchain_.setOnBlockAdded(
          [this](const Block &block)
          { broadcastBlock(block); });

      blockchain_.setOnTransactionAdded(
          [this](const Transaction &tx)
          { broadcastTransaction(tx); });

      std::cout << "[Node] Listening on port " << port_
                << " with nodeId " << nodeIdentity_.record().nodeId.substr(0, 16)
                << "..." << std::endl;
    }

    void Node::start()
    {
      doAccept();
      startHeartbeatTimer();
      startGossipTimer();
      startPeerRotationTimer();
      connectToBootstrapPeers();

      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(2));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
                          if (!ec)
                          {
                            requestChainFromPeers();
                          } });
    }

    void Node::connectToPeer(uint16_t port)
    {
      connectToPeer("127.0.0.1", port);
    }

    std::string Node::makeEndpoint(const std::string &host, uint16_t port) const
    {
      return host + ":" + std::to_string(port);
    }

    void Node::connectToPeer(const std::string &host, uint16_t port, int retryCount)
    {
      if (port == port_)
      {
        return;
      }

      const std::string endpoint = makeEndpoint(host, port);
      if (isAlreadyConnected(host, port))
      {
        return;
      }

      if (!peerManager_.shouldAttemptConnection(endpoint, options_.retryPolicy.reconnectCooldownMs))
      {
        return;
      }

      if (peerManager_.isQuarantined(endpoint))
      {
        return;
      }

      if (peerScorer_.isBanned(endpoint))
      {
        return;
      }

      if (!connectionManager_.canAccept(ConnectionDirection::OUTBOUND))
      {
        const auto dropCandidate =
            connectionManager_.selectLowestScorePeerToDrop(connectedPeerScores());

        if (!dropCandidate.empty())
        {
          std::lock_guard<std::mutex> lock(peersMutex_);
          for (auto &peer : peers_)
          {
            if (peer->getEndpoint() == dropCandidate)
            {
              std::cerr << "[Connection] Dropping low-score peer " << dropCandidate
                        << " to free outbound slot" << std::endl;
              peer->close();
              break;
            }
          }
        }

        if (!connectionManager_.canAccept(ConnectionDirection::OUTBOUND))
        {
          networkMetrics_.incrementRejectedPeers();
          return;
        }
      }

      peerManager_.markDialAttempt(endpoint);

      auto resolver = std::make_shared<tcp::resolver>(ioContext_);
      resolver->async_resolve(
          host, std::to_string(port),
          [this, resolver, host, port, endpoint, retryCount](
              boost::system::error_code ec,
              tcp::resolver::results_type results)
          {
            if (ec)
            {
              retryConnect(host, port, retryCount);
              return;
            }

            auto socket = std::make_shared<tcp::socket>(ioContext_);
            boost::asio::async_connect(
                *socket, results,
                [this, socket, host, port, endpoint, retryCount](
                    boost::system::error_code ec,
                    const tcp::endpoint &)
                {
                  if (ec)
                  {
                    retryConnect(host, port, retryCount);
                    return;
                  }

                  auto peer = std::make_shared<Peer>(
                      std::move(*socket),
                      [this](std::shared_ptr<Peer> p, const Message &msg)
                      {
                        handleMessage(p, msg);
                      });

                  peer->setListenPort(port);
                  peerScorer_.addPeer(host, port);
                  peerManager_.addKnownPeer(endpoint);
                  bootstrapManager_.registerPeer(endpoint);

                  {
                    std::lock_guard<std::mutex> lock(peersMutex_);
                    peers_.push_back(peer);
                  }

                  connectionManager_.registerConnection(endpoint, ConnectionDirection::OUTBOUND);
                  peer->start();

                  sendNodeAnnounce(peer);

                  Message registerMsg;
                  registerMsg.type = MessageType::REGISTER_NODE;
                  registerMsg.payload = {
                      {"endpoint", nodeIdentity_.endpoint()},
                      {"nodeId", nodeIdentity_.record().nodeId}};
                  peer->send(registerMsg);

                  Message bootstrapReq;
                  bootstrapReq.type = MessageType::REQUEST_BOOTSTRAP;
                  bootstrapReq.payload = nlohmann::json::object();
                  peer->send(bootstrapReq);

                  Message reqPeers;
                  reqPeers.type = MessageType::REQUEST_PEERS;
                  reqPeers.payload = {
                      {"requester", nodeIdentity_.endpoint()},
                      {"nodeId", nodeIdentity_.record().nodeId}};
                  peer->send(reqPeers);

                  Message reqChain;
                  reqChain.type = MessageType::REQUEST_CHAIN;
                  reqChain.payload = nlohmann::json::object();
                  peer->send(reqChain);

                  refreshMetrics();
                });
          });
    }

    void Node::retryConnect(const std::string &host, uint16_t port, int retryCount)
    {
      if (retryCount >= options_.retryPolicy.maxRetries)
      {
        const std::string endpoint = makeEndpoint(host, port);
        peerManager_.quarantinePeer(endpoint, options_.timeouts.quarantineSec);
        return;
      }

      const int base = options_.retryPolicy.baseDelayMs;
      const int maxDelay = options_.retryPolicy.maxDelayMs;
      int delayMs = std::min(base * (1 << retryCount), maxDelay);

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> jitterDist(0, std::max(0, options_.retryPolicy.jitterMs));
      delayMs += jitterDist(gen);

      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::milliseconds(delayMs));
      timer->async_wait([this, timer, host, port, retryCount](boost::system::error_code ec)
                        {
                          if (!ec)
                          {
                            connectToPeer(host, port, retryCount + 1);
                          } });
    }

    void Node::broadcastTransaction(const Transaction &tx)
    {
      {
        std::lock_guard<std::mutex> lock(seenMutex_);
        if (seenTxIDs_.count(tx.txID))
        {
          return;
        }
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
        {
          return;
        }
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

    nlohmann::json Node::getNodeIdentity() const
    {
      return nodeIdentity_.toJson();
    }

    nlohmann::json Node::getNetworkStats() const
    {
      auto stats = networkMetrics_.toJson();
      stats["node"] = nodeIdentity_.toJson();
      stats["connectionLimits"] = {
          {"maxOutboundPeers", options_.maxOutboundPeers},
          {"maxInboundPeers", options_.maxInboundPeers},
          {"currentOutboundPeers", connectionManager_.outboundCount()},
          {"currentInboundPeers", connectionManager_.inboundCount()}};
      stats["quarantinedPeers"] = peerManager_.quarantinedPeerCount();
      return stats;
    }

    void Node::doAccept()
    {
      acceptor_.async_accept(
          [this](boost::system::error_code ec, tcp::socket socket)
          {
            if (!ec)
            {
              if (!connectionManager_.canAccept(ConnectionDirection::INBOUND))
              {
                const auto dropCandidate =
                    connectionManager_.selectLowestScorePeerToDrop(connectedPeerScores());
                if (!dropCandidate.empty())
                {
                  std::lock_guard<std::mutex> lock(peersMutex_);
                  for (auto &peer : peers_)
                  {
                    if (peer->getEndpoint() == dropCandidate)
                    {
                      peer->close();
                      break;
                    }
                  }
                }
              }

              if (!connectionManager_.canAccept(ConnectionDirection::INBOUND))
              {
                networkMetrics_.incrementRejectedPeers();
                boost::system::error_code shutEc;
                socket.shutdown(tcp::socket::shutdown_both, shutEc);
                socket.close(shutEc);
                doAccept();
                return;
              }

              auto peer = std::make_shared<Peer>(
                  std::move(socket),
                  [this](std::shared_ptr<Peer> p, const Message &msg)
                  {
                    handleMessage(p, msg);
                  });

              const std::string endpoint = peer->getEndpoint();
              peerManager_.addKnownPeer(endpoint);
              bootstrapManager_.registerPeer(endpoint);
              connectionManager_.registerConnection(endpoint, ConnectionDirection::INBOUND);

              {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_.push_back(peer);
              }

              peer->start();
              sendNodeAnnounce(peer);
              refreshMetrics();
            }
            doAccept();
          });
    }

    void Node::sendNodeAnnounce(std::shared_ptr<Peer> peer)
    {
      Message announce;
      announce.type = MessageType::NODE_ANNOUNCE;
      announce.payload = {
          {"nodeId", nodeIdentity_.record().nodeId},
          {"publicKey", nodeIdentity_.record().publicKey},
          {"publicIp", nodeIdentity_.record().advertisedIp},
          {"port", port_},
          {"endpoint", nodeIdentity_.endpoint()}};
      peer->send(announce);
    }

    void Node::handleMessage(std::shared_ptr<Peer> peer, const Message &msg)
    {
      const std::string peerKey = peer->getEndpoint();

      if (peerScorer_.isBanned(peerKey))
      {
        removePeer(peer, true);
        return;
      }

      if (peer->isRateLimited(100))
      {
        penalizePeerByEndpoint(peerKey, PeerScorer::PENALTY_INVALID_TX);
        return;
      }

      const auto now = std::chrono::steady_clock::now();
      if (msg.type != MessageType::PING && msg.type != MessageType::PONG)
      {
        std::lock_guard<std::mutex> lock(seenMutex_);
        const std::string dedupKey = std::to_string(static_cast<int>(msg.type)) +
                                     ":" + crypto::sha256(msg.payload.dump());
        auto it = messageDedup_.find(dedupKey);
        if (it != messageDedup_.end())
        {
          const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
          if (age < options_.timeouts.dedupWindowSec)
          {
            return;
          }
        }
        messageDedup_[dedupKey] = now;
        for (auto iter = messageDedup_.begin(); iter != messageDedup_.end();)
        {
          const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - iter->second).count();
          if (age > options_.timeouts.dedupWindowSec)
          {
            iter = messageDedup_.erase(iter);
          }
          else
          {
            ++iter;
          }
        }
      }

      peerScorer_.markSeen(peerKey);

      switch (msg.type)
      {
      case MessageType::NODE_ANNOUNCE:
      {
        const std::string publicIp = msg.payload.value("publicIp", "");
        const uint16_t peerPort = static_cast<uint16_t>(msg.payload.value("port", 0));
        const std::string endpoint = msg.payload.value("endpoint", "");

        if (!publicIp.empty() && peerPort > 0)
        {
          peer->setListenPort(peerPort);
          peerScorer_.addPeer(publicIp, peerPort);
          peerManager_.addKnownPeer(makeEndpoint(publicIp, peerPort));
          bootstrapManager_.registerPeer(makeEndpoint(publicIp, peerPort));
        }

        if (!endpoint.empty())
        {
          peerManager_.addKnownPeer(endpoint);
          bootstrapManager_.registerPeer(endpoint);
        }

        break;
      }

      case MessageType::REGISTER_NODE:
      {
        const std::string endpoint = msg.payload.value("endpoint", "");
        if (!endpoint.empty())
        {
          bootstrapManager_.registerPeer(endpoint);
          peerManager_.addKnownPeer(endpoint);
        }

        Message resp;
        resp.type = MessageType::RESPONSE_BOOTSTRAP;
        resp.payload = bootstrapManager_.knownTopology(512);
        peer->send(resp);
        break;
      }

      case MessageType::REQUEST_BOOTSTRAP:
      {
        Message resp;
        resp.type = MessageType::RESPONSE_BOOTSTRAP;
        resp.payload = bootstrapManager_.knownTopology(512);
        peer->send(resp);
        break;
      }

      case MessageType::RESPONSE_BOOTSTRAP:
      {
        handleReceivedPeers(msg.payload);
        break;
      }

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
            const bool accepted = blockchain_.addTransaction(tx);
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
        catch (...)
        {
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
            if (block.index == chain.size() && block.previousHash == chain.back().hash)
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
        catch (...)
        {
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
        catch (...)
        {
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
        std::vector<std::string> peers = peerManager_.getKnownPeers();
        peers.push_back(nodeIdentity_.endpoint());

        for (const auto &bootstrap : bootstrapManager_.knownTopology(256))
        {
          peers.push_back(bootstrap);
        }

        std::sort(peers.begin(), peers.end());
        peers.erase(std::unique(peers.begin(), peers.end()), peers.end());

        Message resp;
        resp.type = MessageType::RESPONSE_PEERS;
        resp.payload = peers;
        peer->send(resp);
        break;
      }

      case MessageType::RESPONSE_PEERS:
      case MessageType::PEER_LIST:
      {
        handleReceivedPeers(msg.payload);
        break;
      }

      default:
        break;
      }

      refreshMetrics();
    }

    void Node::removePeer(std::shared_ptr<Peer> peer, bool quarantine)
    {
      const std::string endpoint = peer->getEndpoint();

      if (quarantine)
      {
        peerManager_.quarantinePeer(endpoint, options_.timeouts.quarantineSec);
      }

      connectionManager_.unregisterConnection(endpoint);

      std::lock_guard<std::mutex> lock(peersMutex_);
      peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                  [&peer](const std::shared_ptr<Peer> &p)
                                  {
                                    return p == peer || !p->isConnected();
                                  }),
                   peers_.end());

      refreshMetrics();
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

    void Node::requestPeersFromAll()
    {
      Message msg;
      msg.type = MessageType::REQUEST_PEERS;
      msg.payload = {
          {"requester", nodeIdentity_.endpoint()},
          {"nodeId", nodeIdentity_.record().nodeId}};
      broadcast(msg);
    }

    void Node::handleReceivedPeers(const nlohmann::json &payload)
    {
      if (!payload.is_array())
      {
        return;
      }

      std::vector<std::string> discovered;
      for (const auto &entry : payload)
      {
        if (!entry.is_string())
        {
          continue;
        }

        const std::string endpoint = entry.get<std::string>();
        if (endpoint.empty() || endpoint == nodeIdentity_.endpoint())
        {
          continue;
        }

        std::string host;
        uint16_t port = 0;
        if (!PeerManager::parseEndpoint(endpoint, host, port))
        {
          continue;
        }

        if (port == port_)
        {
          continue;
        }

        if (peerScorer_.isBanned(endpoint) || peerManager_.isQuarantined(endpoint))
        {
          continue;
        }

        discovered.push_back(endpoint);
      }

      peerManager_.mergeKnownPeers(discovered);
      for (const auto &peer : discovered)
      {
        bootstrapManager_.registerPeer(peer);
      }

      attemptRandomPeerExpansion();
      refreshMetrics();
    }

    void Node::startGossipTimer()
    {
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(options_.timeouts.gossipIntervalSec));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
                          if (!ec)
                          {
                            requestPeersFromAll();
                            attemptRandomPeerExpansion();
                            peerScorer_.evictStalePeers(300);
                            refreshMetrics();
                            startGossipTimer();
                          } });
    }

    void Node::startPeerRotationTimer()
    {
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(options_.timeouts.peerRotationSec));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
                          if (!ec)
                          {
                            attemptRandomPeerExpansion();
                            startPeerRotationTimer();
                          } });
    }

    void Node::connectToBootstrapPeers()
    {
      for (const auto &endpoint : bootstrapManager_.bootstrapPeers())
      {
        std::string host;
        uint16_t port = 0;
        if (!PeerManager::parseEndpoint(endpoint, host, port))
        {
          continue;
        }

        connectToPeer(host, port);
      }
    }

    void Node::attemptRandomPeerExpansion()
    {
      const size_t outboundCount = connectionManager_.outboundCount();
      if (outboundCount >= options_.maxOutboundPeers)
      {
        return;
      }

      std::vector<std::string> exclude = getPeerList();
      exclude.push_back(nodeIdentity_.endpoint());

      const size_t needed = options_.maxOutboundPeers - outboundCount;
      const auto peers = peerManager_.getRandomPeers(std::max<size_t>(needed, 2), exclude);

      for (const auto &endpoint : peers)
      {
        std::string host;
        uint16_t port = 0;
        if (!PeerManager::parseEndpoint(endpoint, host, port))
        {
          continue;
        }

        connectToPeer(host, port);
      }
    }

    void Node::startHeartbeatTimer()
    {
      auto timer = std::make_shared<boost::asio::steady_timer>(
          ioContext_, std::chrono::seconds(options_.timeouts.heartbeatIntervalSec));
      timer->async_wait([this, timer](boost::system::error_code ec)
                        {
                          if (!ec)
                          {
                            Message ping;
                            ping.type = MessageType::PING;
                            ping.payload = nlohmann::json::object();

                            std::vector<std::shared_ptr<Peer>> snapshot;
                            {
                              std::lock_guard<std::mutex> lock(peersMutex_);
                              peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                                          [this](const std::shared_ptr<Peer> &p)
                                                          {
                                                            if (!p->isConnected())
                                                            {
                                                              connectionManager_.unregisterConnection(p->getEndpoint());
                                                              return true;
                                                            }
                                                            return false;
                                                          }),
                                           peers_.end());
                              snapshot = peers_;
                            }

                            for (auto &peer : snapshot)
                            {
                              peerScorer_.markPingSent(peer->getEndpoint());
                              peer->send(ping);
                            }

                            auto timedOutPeers = peerScorer_.getHeartbeatTimeoutPeers(
                                options_.timeouts.heartbeatTimeoutSec);
                            if (!timedOutPeers.empty())
                            {
                              std::lock_guard<std::mutex> lock(peersMutex_);
                              for (const auto &endpoint : timedOutPeers)
                              {
                                for (auto &peer : peers_)
                                {
                                  if (peer->getEndpoint() == endpoint)
                                  {
                                    peer->close();
                                    peerManager_.quarantinePeer(endpoint, options_.timeouts.quarantineSec);
                                  }
                                }
                                connectionManager_.unregisterConnection(endpoint);
                              }
                            }

                            refreshMetrics();
                            startHeartbeatTimer();
                          } });
    }

    bool Node::isAlreadyConnected(const std::string &host, uint16_t port) const
    {
      std::lock_guard<std::mutex> lock(peersMutex_);
      const std::string endpoint = makeEndpoint(host, port);
      for (const auto &peer : peers_)
      {
        if (!peer->isConnected())
        {
          continue;
        }
        if (peer->getEndpoint() == endpoint ||
            peer->getListenPort() == port ||
            peer->getPort() == port)
        {
          return true;
        }
      }
      return false;
    }

    std::vector<std::pair<std::string, int32_t>> Node::connectedPeerScores() const
    {
      std::vector<std::pair<std::string, int32_t>> scores;
      const auto peers = getPeerList();
      for (const auto &endpoint : peers)
      {
        scores.push_back({endpoint, peerScorer_.getScore(endpoint)});
      }
      return scores;
    }

    void Node::refreshMetrics()
    {
      networkMetrics_.setConnectedPeers(getPeerList().size());
      networkMetrics_.setKnownPeers(peerManager_.getKnownPeers().size());

      size_t bannedCount = 0;
      for (const auto &entry : peerScorer_.getAllPeers())
      {
        if (entry.second.banned)
        {
          ++bannedCount;
        }
      }
      networkMetrics_.setBannedPeers(bannedCount);
    }

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
            peer->close();
            peerManager_.quarantinePeer(endpoint, options_.timeouts.quarantineSec);
            connectionManager_.unregisterConnection(endpoint);
            break;
          }
        }
      }
      refreshMetrics();
    }

    void Node::rewardPeerByEndpoint(const std::string &endpoint, int32_t reward)
    {
      peerScorer_.rewardPeer(endpoint, reward);
      refreshMetrics();
    }

  } // namespace network
} // namespace blockchain
