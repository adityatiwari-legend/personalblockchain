# PersonalBlockchain — Complete Build Guide

This guide explains everything that was built, how each module works, and how you can build it yourself from scratch.

---

## Table of Contents

1. [What Was Built](#1-what-was-built)
2. [Prerequisites](#2-prerequisites)
3. [Step-by-Step Build Guide](#3-step-by-step-build-guide)
4. [Module Deep Dive](#4-module-deep-dive)
5. [Running the Network](#5-running-the-network)
6. [Full Transaction Flow Example](#6-full-transaction-flow-example)
7. [How to Extend](#7-how-to-extend)

---

## 1. What Was Built

A **production-structured private blockchain network** consisting of:

| Module          | Files                                                            | Purpose                                                        |
| --------------- | ---------------------------------------------------------------- | -------------------------------------------------------------- |
| **Crypto**      | `sha256.h/cpp`, `ecdsa.h/cpp`                                    | SHA-256 hashing, ECDSA key generation/signing/verification     |
| **Wallet**      | `wallet.h/cpp`                                                   | Key pair management, data signing                              |
| **Transaction** | `transaction.h/cpp`                                              | Transaction creation, signing, verification, replay prevention |
| **Block**       | `block.h/cpp`                                                    | Block structure, Merkle root computation, Proof-of-Work mining |
| **Core**        | `blockchain.h/cpp`                                               | Chain management, mempool, consensus rules                     |
| **State**       | `state_manager.h/cpp`                                            | In-memory state tracking, double-spend detection               |
| **Network**     | `node.h/cpp`, `peer.h/cpp`, `message.h/cpp`, `http_server.h/cpp` | TCP P2P networking, HTTP REST API                              |
| **CLI**         | `main.cpp`                                                       | Command-line entry point                                       |

### Security Features Implemented

- **ECDSA (secp256k1)** digital signatures on every transaction
- **SHA-256** hashing for blocks and Merkle trees
- **Double-spend detection** via state manager
- **Replay attack prevention** via 5-minute timestamp window
- **Block tampering detection** via hash chain validation
- **Chain validation on startup**
- **Peer rate limiting** (50 msg/sec max)
- **Malformed message rejection** (invalid JSON, oversized messages >10MB)

---

## 2. Prerequisites

### For Docker Deployment (Easiest)

- Docker and Docker Compose

### For Local Build

- **C++ Compiler**: GCC 7+ or Clang 5+ (C++17 support required)
- **CMake**: Version 3.16 or higher
- **OpenSSL**: Development libraries (libssl-dev)
- **Boost**: System library (libboost-system-dev)

Install on Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev libboost-system-dev
```

Install on macOS:

```bash
brew install cmake openssl boost
```

---

## 3. Step-by-Step Build Guide

### Step 1: Create the Project Structure

```bash
mkdir personalblockchain && cd personalblockchain
mkdir -p include/{crypto,wallet,transaction,block,core,state,network,utils}
mkdir -p src/{crypto,wallet,transaction,block,core,state,network}
```

### Step 2: Get Dependencies

Download nlohmann/json (header-only JSON library):

```bash
curl -L -o include/utils/json.hpp \
  https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
```

### Step 3: Write the CMakeLists.txt

Create `CMakeLists.txt` in the project root. Key requirements:

- Set C++17 standard
- Find OpenSSL and Boost packages
- Glob all `.cpp` files from `src/`
- Link OpenSSL::Crypto, Boost::system, and Threads

### Step 4: Build the Crypto Layer (Bottom-Up)

**This is the foundation.** Start here because everything else depends on it.

1. **SHA-256** (`include/crypto/sha256.h`, `src/crypto/sha256.cpp`)
   - Use OpenSSL's `EVP_MD_CTX` (the modern EVP API, not the deprecated `SHA256()` function)
   - Return lowercase hex string (64 chars)

2. **ECDSA** (`include/crypto/ecdsa.h`, `src/crypto/ecdsa.cpp`)
   - Use `secp256k1` curve (same as Bitcoin/Ethereum)
   - `generateKeyPair()` → returns hex private + compressed public key
   - `sign()` → SHA-256 hash the message, then ECDSA sign, return DER-encoded hex
   - `verify()` → reconstruct EC_KEY from public key hex, verify signature
   - **IMPORTANT**: Use RAII wrappers (`unique_ptr` with custom deleters) for OpenSSL objects to prevent memory leaks

### Step 5: Build the Wallet

- Wraps ECDSA key pair generation
- Constructor accepts private key hex for wallet restoration (derives public key)
- `signData()` delegates to `crypto::sign()`

### Step 6: Build the Transaction System

- `computeTxID()` = `SHA256(sender + receiver + payload + timestamp)`
- `sign()` signs the txID with sender's private key
- `verify()` recomputes txID, then verifies signature against sender's public key
- `isExpired()` checks if timestamp is >5 minutes old (replay prevention)
- Coinbase transactions (from "COINBASE" sender) skip signature verification

### Step 7: Build the Block Structure

- **Merkle Root**: Build bottom-up by pairwise hashing txIDs. Duplicate last hash if odd count.
- **Block Hash**: `SHA256(index + timestamp + merkleRoot + previousHash + nonce + difficulty)`
- **Mining**: Increment nonce until hash has `difficulty` leading zeros
- **Genesis Block**: index=0, previousHash="0000...0000" (64 zeros)

### Step 8: Build the Blockchain Core

- Chain = `vector<Block>`, starts with genesis
- Mempool = `vector<Transaction>`, pending unconfirmed transactions
- `addTransaction()` validates: signature → replay → double-spend → mempool dedup
- `minePendingTransactions()` creates coinbase tx + mempool txs into new block
- `replaceChain()` implements longest-valid-chain consensus
- Event callbacks (`onBlockAdded_`, `onTransactionAdded_`) for P2P broadcasting

### Step 9: Build the State Manager

- `map<publicKey, set<txID>>` tracks who has sent what
- `set<txID>` for fast double-spend lookup
- All operations are `mutex`-protected (thread-safe)
- `rebuildState()` replays entire chain on chain replacement

### Step 10: Build the P2P Network

1. **Message Protocol** (`message.h/cpp`)
   - Types: NEW_TRANSACTION, NEW_BLOCK, REQUEST_CHAIN, RESPONSE_CHAIN, PING, PONG
   - Length-prefix framing: 4-byte big-endian length + JSON body

2. **Peer** (`peer.h/cpp`)
   - Boost.Asio async TCP connection
   - `doReadHeader()` → `doReadBody()` → process → loop
   - Write queue with `deque<string>` for ordered delivery
   - Rate limiting: sliding window, max 50 msg/sec

3. **Node** (`node.h/cpp`)
   - TCP acceptor on configurable port
   - `handleMessage()` routes by type:
     - `NEW_TRANSACTION` → validate → add to mempool → rebroadcast
     - `NEW_BLOCK` → validate → try append or request full chain → rebroadcast
     - `REQUEST_CHAIN` → send full chain
     - `RESPONSE_CHAIN` → try to replace local chain
   - Deduplication via `seenTxIDs_` and `seenBlockHashes_` sets

4. **HTTP Server** (`http_server.h/cpp`)
   - Raw TCP HTTP (no Boost.Beast dependency)
   - Parses request line + headers, reads Content-Length body
   - Routes: POST `/wallet/create`, `/transaction/send`, `/mine`; GET `/chain`, `/mempool`, `/peers`, `/health`

### Step 11: Write main.cpp

- Parse CLI args: `--port`, `--http-port`, `--peers`, `--difficulty`
- Initialize: Blockchain → P2P Node → HTTP Server
- Multi-threaded ASIO: `hardware_concurrency()` threads
- Signal handling for graceful shutdown

### Step 12: Docker Deployment

- **Dockerfile**: Multi-stage (build with gcc + libs, run with minimal runtime)
- **docker-compose.yml**: 3 nodes on a bridge network, each connecting to peers

---

## 4. Module Deep Dive

### How Proof-of-Work Works

```
nonce = 0
loop:
    hash = SHA256(index + timestamp + merkleRoot + previousHash + nonce + difficulty)
    if hash starts with 'difficulty' zeros:
        break  // Block is mined!
    nonce++
```

With difficulty=2, the hash must start with "00...". Higher difficulty = exponentially more computation.

### How Merkle Trees Work

```
        Root = H(H01 + H23)
       /                    \
  H01 = H(H0 + H1)    H23 = H(H2 + H3)
  /         \           /         \
H0=H(tx0)  H1=H(tx1)  H2=H(tx2)  H3=H(tx3)
```

If there's an odd number of transactions, the last hash is duplicated at that level.

### How Chain Sync Works

```
Node A (chain length 5) connects to Node B (chain length 8)
  A sends: REQUEST_CHAIN
  B sends: RESPONSE_CHAIN (entire chain)
  A validates B's chain
  A replaces own chain (because 8 > 5 and chain is valid)
  A rebuilds state from new chain
```

---

## 5. Running the Network

### Docker (3 Nodes)

```bash
docker-compose up --build -d
docker-compose logs -f
```

HTTP APIs available at:

- Node 1: `http://localhost:8001`
- Node 2: `http://localhost:8002`
- Node 3: `http://localhost:8003`

### Local (Manual)

Open 3 terminals:

```bash
# Terminal 1
./blockchain_node --port 5000 --http-port 8000 --difficulty 2

# Terminal 2
./blockchain_node --port 5001 --http-port 8001 --peers 5000 --difficulty 2

# Terminal 3
./blockchain_node --port 5002 --http-port 8002 --peers 5000,5001 --difficulty 2
```

---

## 6. Full Transaction Flow Example

```bash
# Step 1: Create Wallet A on Node 1
curl -s -X POST http://localhost:8001/wallet/create | python3 -m json.tool
# Output: { "publicKey": "02abc...", "privateKey": "def1..." }

# Step 2: Create Wallet B on Node 1
curl -s -X POST http://localhost:8001/wallet/create | python3 -m json.tool
# Output: { "publicKey": "03xyz...", "privateKey": "789a..." }

# Step 3: Send transaction from A -> B
curl -s -X POST http://localhost:8001/transaction/send \
  -H "Content-Type: application/json" \
  -d '{
    "senderPrivateKey": "def1...",
    "receiverPublicKey": "03xyz...",
    "payload": "Transfer 50 carbon credits"
  }' | python3 -m json.tool

# Step 4: View the mempool (transaction should be pending)
curl -s http://localhost:8001/mempool | python3 -m json.tool

# Step 5: Mine a block (includes pending transactions)
curl -s -X POST http://localhost:8001/mine \
  -H "Content-Type: application/json" \
  -d '{"minerAddress": "02abc..."}' | python3 -m json.tool

# Step 6: Verify the chain on all nodes (should be synced)
curl -s http://localhost:8001/chain | python3 -m json.tool
curl -s http://localhost:8002/chain | python3 -m json.tool
curl -s http://localhost:8003/chain | python3 -m json.tool

# Step 7: Health check
curl -s http://localhost:8001/health | python3 -m json.tool
```

---

## 7. How to Extend

### Replace Consensus

The consensus logic lives in `Blockchain::replaceChain()` and `Block::mineBlock()`.
To implement Proof-of-Stake, PBFT, or Raft:

1. Create a new file `include/consensus/your_consensus.h`
2. Define an interface:
   ```cpp
   class ConsensusEngine {
   public:
       virtual bool validateBlock(const Block& block) = 0;
       virtual void proposeBlock(Block& block) = 0;
       virtual bool shouldAcceptChain(const vector<Block>& chain) = 0;
   };
   ```
3. Replace PoW calls in `Blockchain` with the engine interface

### Add New Transaction Types

The `payload` field is a generic string — you can put any JSON in it:

```json
{
  "type": "carbon_credit_transfer",
  "amount": 50,
  "unit": "tonnes_co2",
  "registry": "verra"
}
```

To add domain-specific validation, extend `Transaction::verify()` or create a `PayloadValidator` interface.

### Add Persistence

Currently all state is in-memory. To add persistence:

1. Add RocksDB or LevelDB as a CMake dependency
2. Create `include/storage/chain_store.h` with `save()` and `load()` methods
3. Call `save()` on every new block, `load()` on startup

### Add Smart Contracts

1. Embed a lightweight scripting engine (Lua, WASM)
2. Store contract code in the transaction payload
3. Execute contract in `StateManager::applyBlock()` after verifying the transaction
