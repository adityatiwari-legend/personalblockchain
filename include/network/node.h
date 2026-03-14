#pragma once

#include "core/blockchain.h"
#include "network/bootstrap_manager.h"
#include "network/connection_manager.h"
#include "network/message.h"
#include "network/network_metrics.h"
#include "network/node_identity.h"
#include "network/peer.h"
#include "network/peer_manager.h"
#include "network/peer_scorer.h"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace blockchain
{
  namespace network
  {

    using boost::asio::ip::tcp;

    struct RetryPolicy
    {
      int maxRetries = 6;
      int baseDelayMs = 500;
      int maxDelayMs = 30000;
      int jitterMs = 400;
      int reconnectCooldownMs = 1200;
    };

    struct NetworkTimeouts
    {
      int heartbeatIntervalSec = 15;
      int heartbeatTimeoutSec = 45;
      int gossipIntervalSec = 30;
      int peerRotationSec = 45;
      int quarantineSec = 120;
      int dedupWindowSec = 90;
    };

    struct NodeOptions
    {
      std::string dataDir = "./data";
      std::string publicIp;
      std::vector<std::string> bootstrapNodes;
      size_t maxOutboundPeers = 8;
      size_t maxInboundPeers = 32;
      RetryPolicy retryPolicy;
      NetworkTimeouts timeouts;
    };

    /**
     * P2P Node — manages TCP connections to peers, handles
     * message routing, chain synchronization, gossip-based peer
     * discovery, peer scoring, and heartbeat monitoring.
     */
    class Node
    {
    public:
      /**
       * @param ioContext Boost ASIO I/O context (shared with HTTP server)
       * @param blockchain Reference to the blockchain instance
       * @param port Port to listen on for P2P connections
       */
      Node(boost::asio::io_context &ioContext, Blockchain &blockchain,
           uint16_t port, const NodeOptions &options = NodeOptions());

      /** Start accepting incoming connections + start heartbeat + gossip timers. */
      void start();

      /** Connect to a peer at the given port (on localhost). */
      void connectToPeer(uint16_t port);

      /** Connect to a peer at the given host:port (with retry). */
      void connectToPeer(const std::string &host, uint16_t port, int retryCount = 0);

      /** Broadcast a transaction to all connected peers. */
      void broadcastTransaction(const Transaction &tx);

      /** Broadcast a newly mined block to all connected peers. */
      void broadcastBlock(const Block &block);

      /** Request the full chain from all connected peers. */
      void requestChainFromPeers();

      /** Get the list of connected peer endpoints. */
      std::vector<std::string> getPeerList() const;

      /** Get the listening port. */
      uint16_t getPort() const { return port_; }

      /** Get startup options used for this node. */
      const NodeOptions &getOptions() const { return options_; }

      /** Read-only view of node identity. */
      nlohmann::json getNodeIdentity() const;

      /** Read-only view of current network stats. */
      nlohmann::json getNetworkStats() const;

      /** Get the peer scorer (for diagnostics / HTTP API). */
      const PeerScorer &getPeerScorer() const { return peerScorer_; }

      /** Penalize a peer (called by Node internally on bad data). */
      void penalizePeerByEndpoint(const std::string &endpoint, int32_t penalty);

      /** Reward a peer (called by Node internally on valid data). */
      void rewardPeerByEndpoint(const std::string &endpoint, int32_t reward);

    private:
      void doAccept();
      void handleMessage(std::shared_ptr<Peer> peer, const Message &msg);
      void removePeer(std::shared_ptr<Peer> peer, bool quarantine = false);
      void broadcast(const Message &msg, std::shared_ptr<Peer> exclude = nullptr);
      void retryConnect(const std::string &host, uint16_t port, int retryCount);
      void sendNodeAnnounce(std::shared_ptr<Peer> peer);
      void connectToBootstrapPeers();
      void attemptRandomPeerExpansion();
      void startPeerRotationTimer();
      void refreshMetrics();

      std::string makeEndpoint(const std::string &host, uint16_t port) const;
      std::vector<std::pair<std::string, int32_t>> connectedPeerScores() const;

      // ─── Gossip-based peer discovery ────────────────────────────────
      /** Request peer lists from all connected peers (gossip pull). */
      void requestPeersFromAll();

      /** Process a received peer list and connect to unknown peers. */
      void handleReceivedPeers(const nlohmann::json &payload);

      /** Periodic gossip timer callback. */
      void startGossipTimer();

      // ─── Heartbeat / PING-PONG ─────────────────────────────────────
      /** Send PING to all connected peers and clean up dead ones. */
      void startHeartbeatTimer();

      // ─── Peer eviction ─────────────────────────────────────────────
      /** Check if a peer endpoint is already connected. */
      bool isAlreadyConnected(const std::string &host, uint16_t port) const;

      boost::asio::io_context &ioContext_;
      tcp::acceptor acceptor_;
      Blockchain &blockchain_;
      uint16_t port_;

      mutable std::mutex peersMutex_;
      std::vector<std::shared_ptr<Peer>> peers_;

      // Track recently seen txIDs and block hashes to prevent re-broadcast loops
      std::mutex seenMutex_;
      std::set<std::string> seenTxIDs_;
      std::set<std::string> seenBlockHashes_;
      std::unordered_map<std::string, std::chrono::steady_clock::time_point> messageDedup_;

      // ─── Gossip & scoring ──────────────────────────────────────────
      PeerScorer peerScorer_;

      NodeOptions options_;
      NodeIdentity nodeIdentity_;
      PeerManager peerManager_;
      BootstrapManager bootstrapManager_;
      ConnectionManager connectionManager_;
      NetworkMetrics networkMetrics_;
    };

  } // namespace network
} // namespace blockchain
