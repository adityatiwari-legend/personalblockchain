/**
 * PersonalBlockchain Node — Main Entry Point
 *
 * Usage:
 *   ./blockchain_node --port <p2p_port> --http-port <http_port>
 *       --peers <host1:port1,host2:port2,...> --difficulty <d>
 *       --data-dir <path> --consensus <pow>
 *
 * Example (local):
 *   ./blockchain_node --port 5000 --http-port 8000 --peers
 * 127.0.0.1:5001,127.0.0.1:5002
 *
 * Example (Docker):
 *   ./blockchain_node --port 5000 --http-port 8000 --peers
 * node2:5000,node3:5000 --data-dir /app/data
 */

#include "consensus/pow_engine.h"
#include "core/blockchain.h"
#include "network/http_server.h"
#include "network/node.h"
#include "storage/leveldb_manager.h"
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
  std::string dataDir = "./data";
  std::string consensus = "pow";
  std::string name = "PersonalBlockchain";
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
    else if (arg == "--data-dir" && i + 1 < argc)
    {
      config.dataDir = argv[++i];
    }
    else if (arg == "--consensus" && i + 1 < argc)
    {
      config.consensus = argv[++i];
    }
    else if (arg == "--name" && i + 1 < argc)
    {
      config.name = argv[++i];
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
          pa.host = peerEntry.substr(0, colonPos);
          pa.port =
              static_cast<uint16_t>(std::stoi(peerEntry.substr(colonPos + 1)));
        }
        else
        {
          pa.host = "127.0.0.1";
          pa.port = static_cast<uint16_t>(std::stoi(peerEntry));
        }
        config.peers.push_back(pa);
      }
    }
    else if (arg == "--help" || arg == "-h")
    {
      std::cout << "PersonalBlockchain Node v2.0.0\n"
                << "Usage: blockchain_node [options]\n\n"
                << "Options:\n"
                << "  --port <port>        P2P port (default: 5000)\n"
                << "  --http-port <port>   HTTP API port (default: 8000)\n"
                << "  --difficulty <d>     Mining difficulty (default: 2)\n"
                << "  --data-dir <path>    Data directory (default: ./data)\n"
                << "  --consensus <type>   Consensus engine: pow (default: pow)\n"
                << "  --name <name>        Chain name (default: PersonalBlockchain)\n"
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
            << "  PersonalBlockchain Node v2.0.0\n"
            << "========================================\n"
            << "  P2P Port:    " << config.p2pPort << "\n"
            << "  HTTP Port:   " << config.httpPort << "\n"
            << "  Difficulty:  " << config.difficulty << "\n"
            << "  Data Dir:    " << config.dataDir << "\n"
            << "  Consensus:   " << config.consensus << "\n"
            << "  Chain Name:  " << config.name << "\n"
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
    // Initialize persistent storage
    blockchain::storage::LevelDBManager storage(config.dataDir);

    // Initialize consensus engine
    blockchain::consensus::PoWEngine powEngine;
    blockchain::consensus::IConsensusEngine *engine = &powEngine;

    // Initialize blockchain with consensus engine + persistence
    blockchain::Blockchain chain(config.difficulty, engine, &storage);

    // Initialize Boost ASIO
    boost::asio::io_context ioContext;
    g_ioContext = &ioContext;

    // Start P2P node
    blockchain::network::Node p2pNode(ioContext, chain, config.p2pPort);
    p2pNode.start();

    // Connect to peers
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

    // Graceful shutdown: persist chain and state to disk
    if (shutdownRequested.load())
    {
      std::cout << "[Main] Persisting chain to disk before shutdown..." << std::endl;
      chain.persistChain();
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
