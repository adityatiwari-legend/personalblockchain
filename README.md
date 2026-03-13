# CryptixChain

A production-hardened, modular private blockchain network built from scratch in C++17.
3-node P2P network with PoW consensus, ECDSA signatures, gossip peer discovery, persistent storage, and a React explorer.

## Features

- **SHA-256 Hashing** — OpenSSL EVP API
- **ECDSA Digital Signatures** — secp256k1 curve (same as Bitcoin)
- **Proof-of-Work Consensus** — Dynamic difficulty retargeting (Bitcoin-style, every N blocks)
- **Merkle Trees** — Per-block transaction integrity verification
- **Persistent Storage** — Crash-safe JSON chain files with atomic writes (survives restarts)
- **P2P Networking** — TCP with length-prefix framing, gossip peer discovery, PING/PONG heartbeat
- **Peer Reputation Scoring** — Automatic banning/unbanning of misbehaving peers
- **REST HTTP API** — 9 endpoints: wallet, transactions, mining, chain, mempool, peers, health
- **Security Hardening** — Signature verification, double-spend detection, replay prevention, rate limiting, coinbase injection prevention, difficulty bypass prevention
- **React Explorer** — Live dashboard with block history, network graph, mining panel, transaction form
- **Docker** — Multi-stage build, 3-node compose network with persistent volumes

## Architecture

```
┌────────────────────────────────────────────────┐
│                 main.cpp (CLI)                 │
├──────────────┬─────────────────────────────────┤
│   HTTP API   │           P2P Node              │
│  (9 routes)  │  (TCP, Gossip, Peer Scoring)    │
├──────────────┴─────────────────────────────────┤
│                Blockchain Core                 │
│     (Chain, Mempool, Consensus Engine)         │
├─────────────────────┬──────────────────────────┤
│    State Manager    │   Persistent Storage     │
│ (UTXO, Double-Spend)│ (chain.json, state.json) │
├──────────┬──────────┴──────────────────────────┤
│  Block   │  Transaction  │       Wallet        │
│ (Merkle, │ (Sign, Verify,│  (Key Pairs,        │
│  PoW)    │  Replay Guard)│   Signing)          │
├──────────┴───────────┴──────────────────────────┤
│              Consensus Engine                   │
│   IConsensusEngine → PoWEngine + DifficultyAdjuster │
├─────────────────────────────────────────────────┤
│                 Crypto Layer                    │
│          (SHA-256, ECDSA secp256k1)             │
└─────────────────────────────────────────────────┘
```

## Quick Start

### Docker (Recommended)

```bash
# Build and start 3-node network
docker compose up --build -d

# Verify all nodes are healthy
curl http://localhost:8001/health
curl http://localhost:8002/health
curl http://localhost:8003/health
```

### Frontend Explorer

```bash
cd blockchain-explorer
npm install
npm run dev
# Open http://localhost:5173
```

### Local Build

**Prerequisites:** CMake 3.16+, C++17 compiler, OpenSSL, Boost (system)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run Nodes Manually

```bash
# Node 1 (bootstrap)
./blockchain_node --port 5000 --http-port 8000 --difficulty 2 --data-dir ./data/node1

# Node 2
./blockchain_node --port 5001 --http-port 8001 --peers 127.0.0.1:5000 --data-dir ./data/node2

# Node 3
./blockchain_node --port 5002 --http-port 8002 --peers 127.0.0.1:5000,127.0.0.1:5001 --data-dir ./data/node3
```

**All CLI flags:**

| Flag                  | Default            | Description                                           |
| --------------------- | ------------------ | ----------------------------------------------------- |
| `--port`              | 5000               | P2P listen port                                       |
| `--http-port`         | 8000               | HTTP API port                                         |
| `--difficulty`        | 2                  | Initial mining difficulty                             |
| `--peers`             | —                  | Comma-separated `host:port` seed peers                |
| `--data-dir`          | `./data`           | Persistent chain storage directory                    |
| `--consensus`         | `pow`              | Consensus engine (`pow`)                              |
| `--target-block-time` | 10                 | Target seconds per block (for difficulty retargeting) |
| `--retarget-window`   | 10                 | Blocks per difficulty retarget window                 |
| `--name`              | PersonalBlockchain | Chain name                                            |

## API Reference

| Method | Endpoint            | Description                                                          |
| ------ | ------------------- | -------------------------------------------------------------------- |
| `POST` | `/wallet/create`    | Generate a new ECDSA wallet (returns public + private key)           |
| `POST` | `/transaction/send` | Submit a signed transaction                                          |
| `POST` | `/mine`             | Mine pending transactions into a block                               |
| `GET`  | `/chain`            | Full chain (name, length, all blocks)                                |
| `GET`  | `/mempool`          | Pending unconfirmed transactions                                     |
| `GET`  | `/peers`            | Connected peer addresses                                             |
| `GET`  | `/peers/scores`     | Peer reputation scores                                               |
| `GET`  | `/health`           | Node status (version, height, difficulty, consensus, peers, mempool) |

## Example Transaction Flow

```bash
# 1. Create two wallets
WALLET_A=$(curl -s -X POST http://localhost:8001/wallet/create)
WALLET_B=$(curl -s -X POST http://localhost:8001/wallet/create)

# Extract keys
PRIV_A=$(echo $WALLET_A | python -c "import sys,json; print(json.load(sys.stdin)['privateKey'])")
PUB_A=$(echo $WALLET_A  | python -c "import sys,json; print(json.load(sys.stdin)['publicKey'])")
PUB_B=$(echo $WALLET_B  | python -c "import sys,json; print(json.load(sys.stdin)['publicKey'])")

# 2. Submit a transaction
curl -X POST http://localhost:8001/transaction/send \
  -H "Content-Type: application/json" \
  -d "{
    \"senderPrivateKey\": \"$PRIV_A\",
    \"senderPublicKey\":  \"$PUB_A\",
    \"receiverPublicKey\": \"$PUB_B\",
    \"payload\": \"Transfer 10 tokens\"
  }"

# 3. Mine a block
curl -X POST http://localhost:8001/mine \
  -H "Content-Type: application/json" \
  -d "{\"minerAddress\": \"$PUB_A\"}"

# 4. Verify the chain synced across all nodes
curl -s http://localhost:8001/chain | python -m json.tool | grep '"index"'
curl -s http://localhost:8002/chain | python -m json.tool | grep '"index"'
curl -s http://localhost:8003/chain | python -m json.tool | grep '"index"'
```

## Project Structure

```
personalblockchain/
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── include/
│   ├── consensus/       # IConsensusEngine, PoWEngine, PoSEngine (skeleton), DifficultyAdjuster
│   ├── core/            # Blockchain class (chain + mempool + consensus integration)
│   ├── block/           # Block structure, Merkle tree, PoW mining
│   ├── transaction/     # Transaction signing, verification, replay guard
│   ├── wallet/          # secp256k1 key pair generation and signing
│   ├── state/           # StateManager (UTXO tracking, double-spend detection)
│   ├── storage/         # LevelDBManager (JSON persistence, atomic writes)
│   ├── network/         # Node (P2P), HttpServer (REST), Peer, PeerScorer, Message
│   ├── crypto/          # SHA-256, ECDSA
│   └── utils/           # nlohmann/json (bundled)
├── src/                 # Implementation files (mirrors include/)
│   └── main.cpp         # CLI entry point
├── blockchain-explorer/ # React + Vite frontend
│   └── src/
│       ├── pages/       # Dashboard.jsx
│       ├── components/  # TransactionForm, MiningPanel, NetworkGraph, ...
│       └── services/    # api.js (Axios client)
├── analyse.md           # Detailed technical report
└── README.md            # This file
```

## Security

| Mechanism                     | Detail                                                                                                                           |
| ----------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| ECDSA signature verification  | Every transaction signed with sender's private key; verified on receipt                                                          |
| Coinbase injection prevention | External `COINBASE` or empty sender public key is hard-rejected                                                                  |
| Double-spend detection        | StateManager tracks all spent txIDs; re-spend rejected at mempool                                                                |
| Replay attack prevention      | 5-minute timestamp window; expired transactions rejected                                                                         |
| Difficulty range enforcement  | Difficulty must be 1–64; prevents bypass-via-zero and OOM attacks                                                                |
| Genesis + chain validation    | Full structural validation on every chain replace: genesis, hash chain, Merkle roots, PoW targets, tx signatures, coinbase count |
| Per-peer rate limiting        | 50 messages/second cap; excess triggers penalty score                                                                            |
| Peer reputation scoring       | New peers start at 100; bad behavior penalizes; score ≤ 0 = banned; auto-unban on recovery                                       |
| Write queue bounding          | Max 1,000 queued messages per peer; overflow dropped                                                                             |
| Message deduplication         | `seenTxIDs_` (10k cap) and `seenBlockHashes_` (1k cap) prevent gossip storms                                                     |
| HTTP body size limit          | 1 MB max request body (returns 413)                                                                                              |
| Safe signal handling          | Signal handler only sets atomics; no mutex acquisition; persistence from main thread                                             |
| Use-after-free prevention     | `async_write` buffers held by `shared_ptr` until write completes                                                                 |

## Peer Scoring

| Event               | Score                             |
| ------------------- | --------------------------------- |
| Registration        | +100 (start)                      |
| Valid block/tx      | +1 (cap 200)                      |
| Invalid block       | −10                               |
| Invalid transaction | −5                                |
| Rate limit exceeded | −5                                |
| 3rd invalid block   | −20 (one-time escalation)         |
| Score ≤ 0           | **BANNED** (immediate disconnect) |
| Score recovers > 20 | **UNBANNED**                      |
| Unseen > 300s       | **EVICTED**                       |

## Consensus: Dynamic Difficulty

Difficulty retargets every `--retarget-window` blocks (default: 10) by comparing actual elapsed time to the target (`--target-block-time` × window). Adjustment is clamped to 4× in either direction and bounded to difficulty [1, 8].

```
new_difficulty = old_difficulty × (target_time / actual_time)
                 clamped to [old/4, old×4], then clamped to [1, 8]
```

## Known Limitations

- Wallets created via `/wallet/create` are **in-memory only** — lost on node restart
- No `/balance/:publicKey` endpoint (ownership tracked internally, not exposed)
- No mempool size cap (rate-limited at peer level, not at the mempool level)
- PoS engine exists as a skeleton — no real staking logic; `--consensus pos` falls back to legacy mode
- Frontend API URL is hardcoded to `localhost:8001`

## License

MIT
