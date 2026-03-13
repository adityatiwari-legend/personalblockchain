# PersonalBlockchain - Current Build Status

Last updated: 2026-03-13

## Project Snapshot

This repository currently contains a working C++ blockchain node backend, Dockerized multi-node setup, and a React-based blockchain explorer dashboard.

## What Has Been Built So Far

### 1. Core Blockchain Backend (C++17)

Implemented modules:

- Crypto layer (SHA-256, ECDSA secp256k1)
- Wallet creation/signing support
- Transaction model with signing and verification
- Block model with Merkle root + PoW mining
- Blockchain core (chain, mempool, validation, mining)
- State manager for spent transaction tracking
- P2P networking (peer-to-peer message exchange)
- HTTP API server over Boost.Asio
- Storage manager for persisted chain/state files

Key files:

- src/main.cpp
- src/core/blockchain.cpp
- src/block/block.cpp
- src/transaction/transaction.cpp
- src/network/node.cpp
- src/network/peer.cpp
- src/network/http_server.cpp
- src/storage/leveldb_manager.cpp

### 2. Consensus Layer

Implemented:

- Pluggable consensus interface
- Proof-of-Work (PoW) engine
- Dynamic difficulty retargeting logic

Scaffolded (not fully production-ready):

- Proof-of-Stake (PoS) skeleton interface implementation

Key files:

- include/consensus/consensus_engine.h
- include/consensus/pow_engine.h
- include/consensus/pos_engine.h
- src/consensus/difficulty_adjuster.cpp

### 3. Security and Validation Logic

Implemented checks:

- Signature verification for normal transactions
- Coinbase submission prevention from external API
- Replay prevention using timestamp expiry window
- Double-spend detection via state tracking
- Mempool deduplication
- Block validation: hash integrity, link integrity, Merkle root, difficulty target, transaction validity

Network protections:

- Message size guard (oversized message rejection)
- Malformed message handling
- Per-peer rate limiting
- Peer scoring, penalties, and ban flow

### 4. Networking Features

Implemented:

- Async TCP peer connections (Boost.Asio)
- Length-prefixed message protocol
- Gossip/broadcast for new blocks and transactions
- Chain request/response synchronization
- Heartbeat (PING/PONG)
- Reconnect with exponential backoff
- Peer list exchange messages (REQUEST_PEERS / RESPONSE_PEERS)

### 5. HTTP API Endpoints

Available endpoints:

- POST /wallet/create
- POST /transaction/send
- POST /mine
- GET /chain
- GET /mempool
- GET /peers
- GET /peers/scores
- GET /health
- GET /status

### 6. Persistence and Storage

Implemented:

- Persistent data directory support via --data-dir
- Chain persisted as JSON files
- State persisted as JSON files
- Atomic write strategy with temp file + rename
- Chain loading and validation at startup
- State rebuild from loaded chain

Notes:

- The storage abstraction is named LevelDBManager, but currently uses file-based JSON persistence.

### 7. DevOps and Deployment

Implemented:

- CMake build configuration for C++17 project
- Docker multi-stage build
- Docker Compose 3-node network
- Separate persistent volume per node

Main infra files:

- CMakeLists.txt
- Dockerfile
- docker-compose.yml

### 8. Frontend Explorer (React + Vite)

Implemented UI modules:

- Dashboard overview and charts
- Latest blocks table
- Transaction form panel
- Mining panel
- Network graph visualization
- Auto-refresh polling from backend APIs

Key frontend files:

- blockchain-explorer/src/pages/Dashboard.jsx
- blockchain-explorer/src/services/api.js
- blockchain-explorer/src/components/TransactionForm.jsx
- blockchain-explorer/src/components/MiningPanel.jsx
- blockchain-explorer/src/components/NetworkGraph.jsx

## Current Known Gaps / In-Progress Areas

- PoS is currently a skeleton, not a full staking consensus.
- Frontend API payload/endpoints are partially mismatched with backend transaction API contract.
- Some frontend data formatting assumptions (like timestamp shape) may need alignment with backend responses.
- Explorer README contains endpoint examples that may not fully match the current backend contract.

## Suggested Next Work Items (for upcoming phase)

1. Align frontend transaction API contract with backend /transaction/send payload.
2. Add end-to-end test flow from explorer -> transaction -> mine -> chain update.
3. Standardize API response contracts and update explorer docs.
4. Decide whether to keep JSON-file persistence or migrate to real LevelDB backend.
5. Expand consensus roadmap (complete PoS or strengthen PoW + governance controls).

## Quick Run Commands

### Backend (local)

- mkdir build
- cd build
- cmake ..
- make -j
- ./blockchain_node --port 5000 --http-port 8000 --difficulty 2

### Multi-node (Docker)

- docker-compose up --build -d

### Frontend explorer

- cd blockchain-explorer
- npm install
- npm run dev
