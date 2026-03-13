#pragma once

#include "core/blockchain.h"
#include "network/message.h"
#include "network/peer.h"
#include "network/peer_scorer.h"

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace blockchain
{
  namespace network
  {

    using boost::asio::ip::tcp;

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
           uint16_t port);

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

      /** Get the peer scorer (for diagnostics / HTTP API). */
      const PeerScorer &getPeerScorer() const { return peerScorer_; }

      /** Penalize a peer (called by Node internally on bad data). */
      void penalizePeerByEndpoint(const std::string &endpoint, int32_t penalty);

      /** Reward a peer (called by Node internally on valid data). */
      void rewardPeerByEndpoint(const std::string &endpoint, int32_t reward);

    private:
      void doAccept();
      void handleMessage(std::shared_ptr<Peer> peer, const Message &msg);
      void removePeer(std::shared_ptr<Peer> peer);
      void broadcast(const Message &msg, std::shared_ptr<Peer> exclude = nullptr);
      void retryConnect(const std::string &host, uint16_t port, int retryCount);

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

      // ─── Gossip & scoring ──────────────────────────────────────────
      PeerScorer peerScorer_;

      // Limit max connected peers to prevent resource exhaustion
      static constexpr size_t MAX_PEERS = 50;
    };

  } // namespace network
} // namespace blockchain
