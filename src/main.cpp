/**
 * PersonalBlockchain Node â€” Main Entry Point
 *
 * Usage:
 *   ./blockchain_node --port <p2p_port> --http-port <http_port>
 *       --peers <host1:port1,host2:port2,...> --difficulty <d>
 *
 * Example (local):
 *   ./blockchain_node --port 5000 --http-port 8000 --peers
 * 127.0.0.1:5001,127.0.0.1:5002
 *
 * Example (Docker):
 *   ./blockchain_node --port 5000 --http-port 8000 --peers
 * node2:5000,node3:5000
 */

#include "core/blockchain.h"
#include "network/http_server.h"
#include "network/node.h"
#include "wallet/wallet.h"

#include <atomic>
#include <boost/asio.hpp>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> running{true};
static std::atomic<bool> shutdownRequested{false};
static boost::asio::io_context *g_ioContext = nullptr;

void signalHandler(int /*signal*/)
{
  // Signal handlers must only touch atomics â€” never acquire mutexes.
  // Persistence will be done from the main thread after io_context stops.
  shutdownRequested.store(true);
  running = false;
  if (g_ioContext)
  {
    g_ioContext->stop();
  }
}

struct PeerAddress
{
  std::string host;
  uint16_t port;
};

struct Config
{
  uint16_t p2pPort = 5000;
  uint16_t httpPort = 8000;
  uint32_t difficulty = 2;
  std::vector<PeerAddress> peers;
};

Config parseArgs(int argc, char *argv[])
{
  Config config;

  for (int i = 1; i < argc; i++)
  {
    std::string arg = argv[i];

    if (arg == "--port" && i + 1 < argc)
    {
      config.p2pPort = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--http-port" && i + 1 < argc)
    {
      config.httpPort = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--difficulty" && i + 1 < argc)
    {
      config.difficulty = static_cast<uint32_t>(std::stoi(argv[++i]));
    }
    else if (arg == "--peers" && i + 1 < argc)
    {
      std::string peersStr = argv[++i];
      std::istringstream ss(peersStr);
      std::string peerEntry;
      while (std::getline(ss, peerEntry, ','))
      {
        if (peerEntry.empty())
          continue;
        PeerAddress pa;
        size_t colonPos = peerEntry.rfind(':');
        if (colonPos != std::string::npos && colonPos > 0)
        {
          // Format: host:port
          pa.host = peerEntry.substr(0, colonPos);
          pa.port =
              static_cast<uint16_t>(std::stoi(peerEntry.substr(colonPos + 1)));
        }
        else
        {
          // Just a port number, default to localhost
          pa.host = "127.0.0.1";
          pa.port = static_cast<uint16_t>(std::stoi(peerEntry));
        }
        config.peers.push_back(pa);
      }
    }
    else if (arg == "--help" || arg == "-h")
    {
      std::cout << "PersonalBlockchain Node\n"
                << "Usage: blockchain_node [options]\n\n"
                << "Options:\n"
                << "  --port <port>        P2P port (default: 5000)\n"
                << "  --http-port <port>   HTTP API port (default: 8000)\n"
                << "  --difficulty <d>     Mining difficulty (default: 2)\n"
                << "  --peers <addrs>      Comma-separated peer addresses\n"
                << "                       Format: host:port or just port\n"
                << "                       Example: node2:5000,node3:5000\n"
                << "  --help               Show this help message\n";
      exit(0);
    }
  }

  return config;
}

int main(int argc, char *argv[])
{
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  Config config = parseArgs(argc, argv);

  std::cout << "========================================\n"
            << "  PersonalBlockchain Node v1.0.0\n"
            << "========================================\n"
            << "  P2P Port:    " << config.p2pPort << "\n"
            << "  HTTP Port:   " << config.httpPort << "\n"
            << "  Difficulty:  " << config.difficulty << "\n"
            << "  Peers:       ";

  if (config.peers.empty())
  {
    std::cout << "(none)";
  }
  else
  {
    for (size_t i = 0; i < config.peers.size(); i++)
    {
      if (i > 0)
        std::cout << ", ";
      std::cout << config.peers[i].host << ":" << config.peers[i].port;
    }
  }
  std::cout << "\n========================================\n"
            << std::endl;

  try
  {
    // Initialize blockchain
    blockchain::Blockchain chain(config.difficulty);

    // Initialize Boost ASIO
    boost::asio::io_context ioContext;
    g_ioContext = &ioContext;

    // Start P2P node
    blockchain::network::Node p2pNode(ioContext, chain, config.p2pPort);
    p2pNode.start();

    // Connect to peers (with a small delay to let the acceptor start)
    for (const auto &peer : config.peers)
    {
      p2pNode.connectToPeer(peer.host, peer.port);
    }

    // Start HTTP API server
    blockchain::network::HttpServer httpServer(ioContext, chain, p2pNode,
                                               config.httpPort);
    httpServer.start();

    // Validate chain on startup
    if (chain.isChainValid())
    {
      std::cout << "[Main] Chain validation: PASSED" << std::endl;
    }
    else
    {
      std::cerr << "[Main] Chain validation: FAILED" << std::endl;
      return 1;
    }

    std::cout << "\n[Main] Node is running. Press Ctrl+C to stop.\n"
              << "[Main] HTTP API: http://localhost:" << config.httpPort << "\n"
              << std::endl;

    // Run I/O context in multiple threads for concurrency
    unsigned int threadCount =
        std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < threadCount - 1; i++)
    {
      threads.emplace_back([&ioContext]()
                           { ioContext.run(); });
    }

    // Main thread also runs the I/O context
    ioContext.run();

    // Wait for threads
    for (auto &t : threads)
    {
      if (t.joinable())
        t.join();
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "[Main] Node stopped." << std::endl;
  return 0;
}
