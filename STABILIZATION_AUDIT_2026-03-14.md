# Stabilization Audit and Changes (2026-03-14)

This report focuses on correctness and production hardening only.

## Scope Reviewed

- Blockchain core integration
- Storage reliability
- Networking integration
- Consensus and difficulty behavior
- HTTP API stability
- Frontend integration safety
- Docker deployment reliability

## What Was Already Implemented

- Pluggable consensus interfaces and PoW engine exist.
- Difficulty retargeting logic exists.
- Persistence manager exists behind LevelDBManager abstraction.
- Peer scoring and gossip message types exist.
- Expanded HTTP API exists.
- React explorer polling exists.
- Docker multi-node deployment exists.

## Gaps Found and Implemented in This Pass

### 1) Blockchain Core Integration

Implemented:

- Consensus-aware chain validation path so active engine rules are enforced, not only structural checks.
- Persisted chain load now validates through current consensus rules before acceptance.
- Chain JSON import path now goes through replaceChain to avoid side-door replacement behavior.

Files:

- include/core/blockchain.h
- src/core/blockchain.cpp

### 2) Storage Hardening

Implemented:

- Startup recovery of leftover .tmp files after crash (best-effort recovery for missing canonical file).
- appendBlock now refuses to overwrite storage when existing chain file is malformed.
- appendBlock now checks new block continuity against persisted tip (index and previousHash).
- Safer hasPersistedChain checks with file/size error handling.
- atomicWrite now uses backup swap pattern with restore fallback on failure.

Files:

- include/storage/leveldb_manager.h
- src/storage/leveldb_manager.cpp

### 3) Networking Integration

Implemented:

- Peer score registration made consistent for inbound message paths.
- Heartbeat tracking added (ping sent, pong received, timeout candidate list).
- Heartbeat timeout-driven disconnect path added.

Files:

- include/network/peer_scorer.h
- src/network/peer_scorer.cpp
- src/network/node.cpp

## Remaining Stabilization Work (Not Yet Implemented)

### Blockchain Core

- Add an atomic state+chain commit model (or journal) to avoid chain/state skew under crash windows.
- Add a guarded rollback path when chain replacement fails after partial persistence attempts.
- Add explicit invariants around difficulty transitions across reorgs.

### Storage

- Add checksum/version header per persisted file.
- Add startup detection and quarantine for chain/state mismatch.
- Add fsync-equivalent durability strategy per platform.

### Network

- Add bounded dedup with insertion-order eviction (current set-based eviction is bounded but not true LRU/FIFO).
- Add stronger anti-false-positive ban policy with decay windows and offense classes.
- Add bounded inbound message queue/backpressure metrics.

### Consensus and Difficulty

- Add median-time-past style timestamp checks (not only previous block + future bound).
- Validate expected difficulty schedule in full-chain validation path.
- Add explicit compatibility contract tests for future PoS engine implementation.

### API Hardening

- Standardize error envelope schema across all endpoints.
- Add payload schema validation for all POST routes.
- Add request correlation id and structured error codes.

### Frontend

- Add API schema guards per endpoint and fallback mapping for partial payloads.
- Add exponential backoff after repeated failures instead of fixed-interval polling.
- Add cancellation/abort handling for in-flight requests on page transitions.

### Docker Deployment

- Add startup retries with jittered peer bootstrap strategy.
- Add explicit config checksum/print at startup to detect drift.
- Add volume corruption startup checks and recovery policy.

## Required Deliverables

### 1. Critical Bugs to Fix First (Severity Order)

1. Consensus bypass risk on chain acceptance paths (partially fixed in this pass).
2. Storage overwrite-on-corruption risk in append path (fixed in this pass).
3. Dead peer retention / weak heartbeat liveness handling (fixed in this pass).
4. Chain/state crash consistency gaps (still open).

### 2. Stability Improvements Needed (Importance Order)

1. Atomic persistence model for chain + state.
2. Deterministic validation of retarget schedule under reorg.
3. Network ban-policy calibration with decay windows.
4. API-wide schema/error consistency.
5. Frontend adaptive polling and abort semantics.

### 3. Security Fixes Needed

- Enforce strict request-body schema validation on all mutable endpoints.
- Add endpoint-level authn/authz before production exposure.
- Add payload size limits and type checks uniformly on all POST handlers.
- Add anti-timestamp manipulation hardening (MTP-like checks).

### 4. Testing Gaps

- No automated crash-recovery tests for storage.
- No reorg/replaceChain adversarial tests.
- No heartbeat timeout and ban-policy regression tests.
- No API contract tests shared with frontend.
- No Docker multi-node integration soak tests.

### 5. Refactoring Needed Before New Features

- Centralize chain validation pipeline (single authoritative path).
- Separate persistence transaction concerns from blockchain state mutation.
- Extract network peer identity normalization and scoring bridge into dedicated module.
- Normalize API response helpers to reduce per-route inconsistency.

### 6. Safe Next Milestone After Stabilization

- "Stability Milestone S1":
  - pass crash-recovery and reorg test suite
  - pass 24h multi-node soak with no memory growth regressions
  - stable API contract snapshot consumed by explorer
  - deterministic startup and peer bootstrap behavior in Docker

## Build/Verification Status

- Attempted full CMake configure/build.
- Blocked by local dependency issue: OpenSSL not found on this Windows environment.
- Error context: CMake reports missing OPENSSL_CRYPTO_LIBRARY and OPENSSL_INCLUDE_DIR.

## Files Changed in This Pass

- include/core/blockchain.h
- src/core/blockchain.cpp
- include/storage/leveldb_manager.h
- src/storage/leveldb_manager.cpp
- include/network/peer_scorer.h
- src/network/peer_scorer.cpp
- src/network/node.cpp
