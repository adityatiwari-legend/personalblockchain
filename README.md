# PersonalBlockchain

A production-structured, modular private blockchain network in C++17.

## Features

- **SHA-256 Hashing** — OpenSSL EVP API
- **ECDSA Digital Signatures** — secp256k1 curve (same as Bitcoin)
- **Proof-of-Work Consensus** — Configurable difficulty
- **Merkle Trees** — Transaction integrity verification
- **P2P Networking** — TCP with length-prefix framing (Boost.Asio)
- **REST API** — HTTP endpoints for wallet, transactions, mining, chain
- **Security** — Signature verification, double-spend detection, replay prevention, rate limiting
- **Docker** — Multi-stage build, 3-node compose network

## Architecture

```
┌─────────────────────────────────────────┐
│              main.cpp (CLI)             │
├──────────┬──────────────────────────────┤
│ HTTP API │         P2P Node             │
│ (REST)   │  (TCP, Message Protocol)     │
├──────────┴──────────────────────────────┤
│              Blockchain Core            │
│  (Chain, Mempool, Consensus Rules)      │
├─────────────────────────────────────────┤
│          State Manager                  │
│  (Ownership Tracking, Double-Spend)     │
├──────────┬──────────┬───────────────────┤
│  Block   │Transaction│    Wallet        │
│ (Merkle, │(Sign,     │  (Key Pairs,     │
│  PoW)    │ Verify)   │   Signing)       │
├──────────┴──────────┴───────────────────┤
│           Crypto Layer                  │
│    (SHA-256, ECDSA secp256k1)           │
└─────────────────────────────────────────┘
```

## Quick Start

### Docker (Recommended)

```bash
# Build and start 3-node network
docker-compose up --build -d

# Check health
curl http://localhost:8001/health
curl http://localhost:8002/health
curl http://localhost:8003/health
```

### Local Build

**Prerequisites:** CMake 3.16+, C++17 compiler, OpenSSL, Boost (system)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Nodes

```bash
# Terminal 1
./blockchain_node --port 5000 --http-port 8000 --difficulty 2

# Terminal 2
./blockchain_node --port 5001 --http-port 8001 --peers 5000 --difficulty 2

# Terminal 3
./blockchain_node --port 5002 --http-port 8002 --peers 5000,5001 --difficulty 2
```

## API Reference

| Method | Endpoint            | Description               |
| ------ | ------------------- | ------------------------- |
| POST   | `/wallet/create`    | Create new wallet         |
| POST   | `/transaction/send` | Submit signed transaction |
| POST   | `/mine`             | Mine pending transactions |
| GET    | `/chain`            | View full chain           |
| GET    | `/mempool`          | View pending transactions |
| GET    | `/peers`            | View connected peers      |
| GET    | `/health`           | Health check              |

## Example Transaction Flow

```bash
# 1. Create two wallets
WALLET_A=$(curl -s -X POST http://localhost:8001/wallet/create)
WALLET_B=$(curl -s -X POST http://localhost:8001/wallet/create)

echo $WALLET_A | python -m json.tool
echo $WALLET_B | python -m json.tool

# 2. Send a transaction (use keys from step 1)
curl -X POST http://localhost:8001/transaction/send \
  -H "Content-Type: application/json" \
  -d '{
    "senderPrivateKey": "<SENDER_PRIVATE_KEY>",
    "receiverPublicKey": "<RECEIVER_PUBLIC_KEY>",
    "payload": "Transfer 10 carbon credits"
  }'

# 3. Mine a block
curl -X POST http://localhost:8001/mine \
  -H "Content-Type: application/json" \
  -d '{"minerAddress": "<MINER_PUBLIC_KEY>"}'

# 4. View the chain on all nodes
curl http://localhost:8001/chain | python -m json.tool
curl http://localhost:8002/chain | python -m json.tool
curl http://localhost:8003/chain | python -m json.tool
```

## Project Structure

```
personalblockchain/
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── include/
│   ├── crypto/          # SHA-256 + ECDSA
│   ├── wallet/          # Key management
│   ├── transaction/     # Transaction signing & verification
│   ├── block/           # Block structure, Merkle, PoW
│   ├── core/            # Blockchain logic & consensus
│   ├── state/           # State management
│   ├── network/         # P2P + HTTP server
│   └── utils/           # nlohmann/json (bundled)
├── src/                 # Implementation files
│   └── main.cpp         # CLI entry point
├── GUIDE.md             # Detailed build-from-scratch guide
└── README.md            # This file
```

## Security Features

- **ECDSA Signature Verification** — Every transaction is cryptographically signed
- **Double-Spend Detection** — State manager tracks spent transaction IDs
- **Replay Attack Prevention** — 5-minute timestamp window on transactions
- **Block Tampering Detection** — Full hash chain + Merkle root validation
- **Chain Validation on Startup** — Entire chain verified before accepting peers
- **Peer Rate Limiting** — Max 50 messages/second per peer
- **Malformed Message Rejection** — Invalid messages dropped, oversized (>10MB) rejected

## License

MIT
