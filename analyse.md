Technical Progress Report — PersonalBlockchain (CryptixChain)
Date: March 13, 2026 | Status: v2.0.0 — Production-hardened prototype, post-audit

1. Architecture
   The project implements a multi-node blockchain network written in C++17, built as a single binary node executable (blockchain_node). The architecture is strictly layered:

- **Core**: Block, Blockchain, Transaction, StateManager — chain logic, mempool, UTXO tracking
- **Consensus**: Pluggable engine interface (IConsensusEngine) with a full PoW implementation and a PoS skeleton
- **Storage**: JSON-file persistence layer with crash-safe atomic writes (write-to-tmp + rename)
- **Network**: Async TCP P2P (Boost.Asio) with gossip-based peer discovery, reputation scoring, and a custom HTTP/1.1 REST API server
- **Wallet**: secp256k1 ECDSA key generation, signing, and verification via OpenSSL
- **Frontend**: React + Vite blockchain explorer with real-time polling and mining panel

Each layer has clean header/implementation separation under `include/` and `src/`. The build system is CMake 3.16+, and the binary exposes two ports — P2P (default 5000) and HTTP REST API (default 8000) — configured entirely via CLI flags.

2. Technologies Used
   | Component | Technology |
   |------------------|---------------------------------------------------------------------------|
   | Language | C++17 |
   | Build | CMake 3.16+ |
   | Cryptography | OpenSSL EVP API (SHA-256), secp256k1 curve (ECDSA) |
   | Networking | Boost.Asio 1.70+ — async TCP, no third-party HTTP library |
   | Serialization | nlohmann/json (header-only, bundled) |
   | Frontend | React + Vite, Recharts, react-force-graph-2d, Framer Motion, Axios |
   | Containerization | Docker (multi-stage Ubuntu 22.04), Docker Compose (3-node bridge network) |

The HTTP server is implemented from scratch over raw Boost.Asio TCP — no Boost.Beast or similar — with manual HTTP/1.1 request parsing, CORS headers, content-length body handling, and request size limits (1 MB max body).

3. Security Mechanisms — Implemented & Enforced

**Transaction validation pipeline (5 stages)**
All five checks are enforced sequentially inside `Blockchain::addTransaction()` before any transaction reaches the mempool:

1. **Coinbase injection prevention** — External transactions with `senderPublicKey == "COINBASE"` are hard-rejected. Empty sender public keys are also rejected (fixed: previously allowed as implicit coinbase). Coinbase transactions are only created internally during `minePendingTransactions()`.
2. **ECDSA signature verification** — Every transaction is signed with the sender's secp256k1 private key. `tx.verify()` uses OpenSSL to verify the hex-encoded signature against the sender's compressed public key.
3. **Replay attack prevention** — `tx.isExpired(300)` enforces a 5-minute timestamp window. Transactions older than 5 minutes are rejected outright.
4. **Double-spend detection** — `StateManager` maintains a global `std::set<std::string> spentTxIDs_`. Every confirmed txID is inserted on block confirmation. Any mempool submission that matches a spent txID is rejected.
5. **Mempool deduplication** — A second lookup prevents a transaction from appearing in the mempool twice.

**P2P layer hardening**

- Per-peer rate limiting — `peer->isRateLimited(50)` caps message volume per peer per second
- Message deduplication — `seenTxIDs_` and `seenBlockHashes_` sets prevent gossip re-broadcast storms, bounded at 10,000 and 1,000 entries respectively with eviction to prevent unbounded memory growth
- Write queue bounding — Each peer connection has a max write queue of 1,000 messages; excess messages are dropped to prevent memory exhaustion from slow/malicious peers
- Peer scoring & banning — Reputation system penalizes peers for invalid blocks/transactions, bans at score threshold, with automatic unbanning if score recovers
- Inbound connection limit — MAX_PEERS (50) enforced for both inbound and outbound connections

**HTTP API hardening**

- Request body size limit: 1 MB (returns 413 Payload Too Large)
- Malformed Content-Length handling (returns 400 Bad Request)
- All GET handlers wrapped in try/catch with 500 Internal Server Error fallback
- Use-after-free in async_write eliminated via shared_ptr lifetime management
- CORS headers on all responses

**Chain validation hardening**

- Genesis block validation: hash integrity, merkle root, previousHash must be "0"
- Difficulty range enforcement: 1–64 (prevents 0-difficulty bypass and OOM from extreme values)
- Block index continuity check
- Coinbase validation: exactly 1 coinbase per block, must be first transaction
- Consensus engine validation per block (PoW: hash target, timestamp anti-manipulation)

4. Networking Layer
   | Aspect | Detail |
   |-------------------|---------------------------------------------------------------------------|
   | Transport | TCP with length-prefix framing (4-byte big-endian header, 10 MB max message) |
   | Message protocol | 9 typed messages: NEW_TRANSACTION, NEW_BLOCK, REQUEST_CHAIN, RESPONSE_CHAIN, PING, PONG, REQUEST_PEERS, RESPONSE_PEERS, PEER_LIST |
   | Connection model | Each peer runs as a shared `Peer` object with async read/write loops |
   | Reconnection | Exponential backoff (2 → 4 → 8 → 16 → 30s, max 5 retries) |
   | Peer discovery | Gossip-based — `REQUEST_PEERS` broadcast every 30 seconds; peers share only reputable peers (score > 20) |
   | Heartbeat | PING/PONG every 15 seconds; dead peers cleaned up on timeout |
   | Stale eviction | Peers unseen for 300 seconds are evicted from the scorer |
   | Peer seeding | Static via `--peers host:port,host:port` CLI flag; gossip extends the mesh dynamically |

The node broadcasts new transactions and blocks via Blockchain event callbacks wired up in the Node constructor — the network layer reacts to blockchain events rather than polling.

**Signal handling**: Safe shutdown uses only atomics in the signal handler. No mutex acquisition, no I/O. Persistence is performed from the main thread after `io_context.run()` returns.

5. Consensus
   | Aspect | Detail |
   |---------------------|--------------------------------------------------------------------------|
   | Interface | `IConsensusEngine` — pluggable abstract class with `validateBlock()`, `sealBlock()`, `shouldAcceptChain()`, `nextDifficulty()` |
   | Active engine | Proof of Work (PoW) |
   | Hash function | `SHA256(index + timestamp + merkleRoot + previousHash + nonce + difficulty)` |
   | Target | Nonce is incremented until the hash has `difficulty` leading zero characters |
   | Difficulty | Dynamic retargeting every N blocks (default: 10), measuring actual vs target block time (default: 10s) |
   | Adjustment limits | Clamped to 4x change per window, bounded to [1, 8] difficulty range |
   | Anti-manipulation | Timestamps must be monotonically non-decreasing and not more than 2 minutes in the future |
   | Chain selection | Longest-valid-chain rule (Nakamoto consensus) |
   | Chain validation | Genesis structure, sequential previousHash linking, per-block PoW target, Merkle root integrity, all transaction ECDSA signatures, coinbase correctness |
   | CLI tuning | `--difficulty`, `--target-block-time`, `--retarget-window`, `--consensus` |

**PoS skeleton**: A `PoSEngine` class exists and compiles but has no real staking logic — no stake registry, no validator selection, no block signing, no slashing. It is not selectable from the CLI.

6. Persistent Storage
   | Aspect | Detail |
   |-------------------|---------------------------------------------------------------------------|
   | Backend | JSON file-based (not actual LevelDB, despite the class name) |
   | Chain file | `<data-dir>/chain.json` — full chain as a JSON array of block objects |
   | State file | `<data-dir>/state.json` — spent txID cache (fast-path; state is always rebuildable from chain) |
   | Crash safety | Atomic writes: write to `.tmp` file, then `fs::rename` to target |
   | Operations | `loadChain()`, `saveChain()`, `appendBlock()`, `saveState()`, `loadState()` |
   | Thread safety | Mutex-guarded on all public methods |
   | Startup flow | Load chain from disk → validate integrity → rebuild state from chain → serve |
   | Shutdown flow | Signal sets atomic → io_context stops → threads join → `persistChain()` from main thread |
   | Docker volumes | Named volumes (`node1_data`, `node2_data`, `node3_data`) survive container restarts |

7. Transaction Validation Logic
   Five-stage pipeline (detailed in Section 3). The payload field is a freeform string — any data can be embedded. There is no fee mechanism; all valid transactions are included in the next mined block regardless of priority.

8. Node Synchronization
   Chain sync occurs in three situations:

9. **On node start** — `requestChainFromPeers()` fires 2 seconds after startup, broadcasting `REQUEST_CHAIN` to all connected peers
10. **On peer connect** — The connecting side sends both `REQUEST_CHAIN` and `REQUEST_PEERS` immediately after the TCP handshake
11. **On NEW_BLOCK receipt** — If the incoming block index equals `chain.size()` and `previousHash` matches, the block is appended. If the incoming block index is ahead of the local tip, the node requests the full chain from that peer

State is fully rebuilt (`stateManager_.rebuildState(chain_)`) whenever `replaceChain()` succeeds. The mempool is purged of any transactions that appear in the new chain using `getAllSpentTxIDs()`.

9. HTTP API Endpoints
   | Method | Path | Description |
   |-----------|-------------------|------------------------------------------------------------------|
   | `OPTIONS` | `*` | CORS preflight |
   | `POST` | `/wallet/create` | Generate new ECDSA wallet (returns public + private key) |
   | `POST` | `/transaction/send` | Submit a signed transaction (senderPrivateKey, senderPublicKey, receiverPublicKey, payload) |
   | `POST` | `/mine` | Mine pending transactions (optional minerAddress) |
   | `GET` | `/chain` | Full chain (name, length, blocks) |
   | `GET` | `/mempool` | Pending transactions |
   | `GET` | `/peers` | Connected peer list |
   | `GET` | `/peers/scores` | Peer reputation scores |
   | `GET` | `/health` | Node health: version, chain length, difficulty, consensus, peer count, mempool size |

10. Frontend (Blockchain Explorer)

- **Framework**: React + Vite with Tailwind CSS
- **Dashboard**: Real-time blockchain height, area chart of tx/block, latest blocks table, recent activity feed
- **Transaction Form**: Wallet-aware form with sender private key, sender public key, receiver public key, and payload fields
- **Mining Panel**: One-click block mining with animated feedback and result display
- **Network Graph**: Force-directed graph visualization of connected peers (react-force-graph-2d)
- **Polling**: `Promise.allSettled`-based resilient polling every 4 seconds — individual endpoint failures don't block other data
- **Error handling**: Backend error messages surfaced to user via toast notifications
- **API client**: Axios-based, pointing at `localhost:8001` (node1's HTTP port as mapped by docker-compose)

11. Docker Deployment

- **Dockerfile**: Multi-stage build on Ubuntu 22.04 — installs deps, builds with CMake, copies binary to runtime image
- **docker-compose.yml**: 3-node cluster (`node1`, `node2`, `node3`)
  - Internal ports: 5000 (P2P), 8000 (HTTP)
  - Host-mapped ports: 5001–5003 (P2P), 8001–8003 (HTTP)
  - Named persistent volumes per node
  - Shared bridge network (`blockchain_net`)
  - `restart: unless-stopped`
  - Each node peers with the other two via `--peers`

12. Peer Scoring System
    | Event | Score Change | Notes |
    |---------------------------|-------------|----------------------------------------|
    | Initial registration | +100 | Starting score for new peers |
    | Valid block received | +1 | Capped at 200 |
    | Valid transaction received | +1 | Capped at 200 |
    | Invalid block | -10 | Tracked for rapid misbehavior detection |
    | Invalid transaction | -5 | |
    | 3rd invalid block | -20 | One-time rapid misbehavior escalation |
    | Rate limit exceeded | -5 | |
    | Score ≤ 0 | BANNED | Peer disconnected immediately |
    | Score recovers > 20 | UNBANNED | Peer can reconnect |
    | Unseen for 300s | EVICTED | Removed from scorer entirely |
    | Gossip sharing threshold | Score > 20 | Only reputable peers are shared |

13. Current Limitations
    | Limitation | Impact |
    |-------------------------------------|---------------------------------------------------------------|
    | In-memory wallets only | Private keys created via `/wallet/create` are lost on restart |
    | No transaction fees | No priority mechanism for mempool inclusion |
    | No balance query API | StateManager tracks ownership but doesn't expose balances via HTTP |
    | PoS is skeleton only | `--consensus pos` falls back to legacy mode; no real staking |
    | Frontend hardcoded to localhost:8001| Multi-node or remote deployment requires manual URL changes |
    | No public deployment | Accessible only via Docker Compose locally |
    | No mempool size cap | Mempool can grow unbounded under sustained spam (rate-limited at peer level but not at mempool level) |
    | Raw `this` capture in async lambdas | Node lifetime management could be strengthened with `shared_from_this` in some code paths |

14. Audit Fixes Applied (v2.0.0)
    The following critical fixes were applied during a comprehensive production audit:

**Critical Bug Fixes**

- BUG-1: Use-after-free in HTTP `async_write` — local buffer replaced with `shared_ptr` lifetime
- BUG-2: Signal handler deadlock — removed mutex acquisition from signal handler; persistence moved to main thread
- BUG-3: Empty senderPublicKey coinbase bypass — removed `|| senderPublicKey.empty()` from `isCoinbase()`
- BUG-4: Difficulty=0 bypass — enforced difficulty ≥ 1 in chain validation
- BUG-5: Self-asserted difficulty — consensus engine validates difficulty per block
- BUG-6: Genesis block not validated — full genesis validation including hash, merkle root, previousHash
- BUG-7: Coinbase count not validated — enforced exactly 1 coinbase per block, must be first transaction
- BUG-8: Ephemeral port identity — peer identity now uses known listen port, not TCP ephemeral port
- BUG-9: No inbound connection limit — MAX_PEERS enforced for inbound connections
- BUG-10: Frontend API contract mismatch — TransactionForm, MiningPanel, and api.js aligned with backend

**Security & Stability**

- SEC-5: Content-Length upper bound (1 MB) with proper 413 response
- HTTP malformed request handling (400 responses)
- GET handler exception safety (try/catch returning 500)
- Write queue bounding per peer (1,000 messages max)
- Seen-message set eviction (prevents unbounded memory growth)
- Peer scoring unban mechanism (recoverable bans)
- Rapid misbehavior escalation fix (one-time trigger at threshold)
- `isChainValid` converted from static to instance method for consensus engine integration
- Thread-safe callback setters on Blockchain
- Persistence error handling (return values checked, warnings logged)
- Dashboard polling resilience (Promise.allSettled instead of Promise.all)
- Backend error messages surfaced to frontend toast notifications

15. Future Roadmap (Prioritised)
1. **Wallet persistence** — Encrypted keystore file so wallets survive restarts
1. **Balance query API** — `GET /balance/:publicKey` endpoint exposing StateManager's ownership map
1. **Mempool size cap** — Reject transactions when mempool exceeds configured limit
1. **Proof-of-Stake consensus** — Implement real staking logic in PoSEngine (stake registry, validator selection, block signing, slashing)
1. **Node lifetime management** — Replace raw `this` captures in async lambdas with `shared_from_this` throughout Node
1. **Token economy / smart contracts** — Typed transaction payloads, asset balances, scripting
1. **Blockchain explorer enhancements** — Multi-node view, transaction search, wallet balance queries, configurable API URL
1. **Public deployment** — Cloud VM or container orchestration with public endpoints

---

Simplified Public-Facing Summary

CryptixChain is a private blockchain network built entirely from scratch in C++. It implements the core mechanics of how public blockchains like Bitcoin work — including ECDSA cryptographic signatures, Proof-of-Work mining with dynamic difficulty retargeting, peer-to-peer networking with gossip-based discovery and reputation scoring, persistent chain storage with crash-safe writes, and tamper-proof chain validation — running as a 3-node network inside Docker containers.

Every transaction is signed using the same elliptic curve cryptography (secp256k1) that Bitcoin uses. Every block is mined by solving a computational puzzle that adjusts in difficulty based on network hashrate. Every node independently verifies the full chain — including genesis structure, hash linkage, Merkle roots, PoW targets, and all transaction signatures — before accepting updates from peers. Malicious peers are automatically scored, penalized, and banned. And every component — from the hash function to the HTTP API to the peer discovery protocol — is written in C++, without relying on any existing blockchain framework.

It includes a live web explorer where you can watch blocks being mined, see the network topology, submit transactions, and create wallets in real time.

Built solo, as a prototype for a real product.

---

LinkedIn Post

I built a blockchain network from scratch in C++. Here's what that actually means technically.

Not a tutorial clone. Not an Ethereum fork. A full implementation of core blockchain mechanics — written from scratch in C++17, production-hardened through a comprehensive security audit.

What's working today:

- ECDSA digital signatures (secp256k1 — same curve as Bitcoin) — every transaction is cryptographically signed and verified before it touches the mempool
- SHA-256 Proof of Work with dynamic difficulty retargeting — difficulty adjusts every N blocks based on actual vs target block time, with anti-manipulation safeguards
- Merkle tree — transaction integrity is verified per-block using a bottom-up pairwise hash tree
- 5-layer transaction validation pipeline — coinbase injection prevention → signature verification → replay attack prevention (5-min expiry window) → double-spend detection → mempool deduplication
- Longest-valid-chain consensus — nodes resolve forks by accepting only chains that are longer and cryptographically valid, with full per-block consensus engine validation
- Gossip-based peer discovery with reputation scoring — peers discover each other dynamically, are rated on behavior, and are automatically banned for sending invalid data
- Persistent chain storage — crash-safe atomic writes, chain survives restarts, state is rebuilt from chain on load
- Async P2P networking built on Boost.Asio — TCP with length-prefix framing, exponential backoff reconnection, per-peer rate limiting, write queue bounding, and message dedup to prevent gossip storms
- REST HTTP API — built over raw TCP (no frameworks), with 9 endpoints covering wallet creation, transaction submission, mining, chain/mempool/peer inspection, and health monitoring
- 3-node Docker Compose network — multi-stage build, persistent volumes, all nodes auto-connect, discover peers via gossip, and sync chains on startup
- React live explorer — resilient polling, real-time block history, network graph, mining panel, and wallet-aware transaction form

The honest part: I used AI extensively during the build. But understanding why each piece exists — why you need a Merkle root, why replay prevention requires a timestamp window, why replaceChain() must validate before accepting, why signal handlers can't acquire mutexes — that understanding is mine, and it's the part that matters.

What's next:

- Encrypted wallet persistence
- Balance query API
- Proof-of-Stake consensus (interface is built, staking logic pending)
- Public deployment

If you're learning how blockchains actually work at the protocol level — skip the online courses. Build one.

GitHub: [link]

#Blockchain #CPlusPlus #SystemsEngineering #Cryptography #P2P #OpenSource #BuildInPublic

---

Note on deployment: To go public, deploy the Docker Compose stack to a cloud VM (AWS EC2, DigitalOcean Droplet, Hetzner) with open inbound ports 5001-5003 and 8001-8003, then point the frontend API_BASE_URL at the VM's public IP. Chain data persists across container restarts via named Docker volumes.
