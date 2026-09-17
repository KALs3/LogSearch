# High-Throughput Log Aggregator & Search Engine

A custom, zero-dependency log aggregation and search engine built from scratch in C++17. Designed to ingest high-volume log streams over network sockets and provide fast keyword and boolean search across millions of records — without using any external database engines or libraries.

**Status:** Active development. Phase 2 of 4 complete. Core ingestion pipeline is operational at 100K+ logs/sec.

---

## Why This Project

Modern log aggregation systems like Elasticsearch, Splunk, and Loki are powerful — but they hide enormous complexity behind their APIs. This project rebuilds the core concepts from scratch to understand *why* these systems are designed the way they are:

- How inverted indexes deliver O(1) term lookup instead of O(N) scans
- How LSM-style append-only segments convert random writes into sequential I/O
- How bounded buffers and worker pools decouple network I/O from processing
- How sorted posting lists enable linear-time boolean intersections

The result is a working system with concrete performance numbers and a fully documented architecture — no black boxes.

---

## Architecture Overview

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  TCP Server  │────▶│  Ring Buffer │────▶│ Worker Pool  │────▶│ Inverted     │
│  (Ingestion) │     │  (Bounded)   │     │ (Parallel)   │     │ Index        │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
       │                    │                    │                     │
   Accepts              Decouples            Parses &              Indexes &
   connections          I/O from CPU         indexes in             stores for
   & reads logs         work                 parallel               fast search
                                                                          │
                                                                          ▼
                                                                   ┌──────────────┐
                                                                   │ Disk Segments│
                                                                   │  (Phase 3)   │
                                                                   └──────────────┘
```

### Design Principles

| Principle | Application |
|-----------|-------------|
| **Zero dependencies** | Only C++ standard library — no Elasticsearch, Lucene, Redis, or SQLite |
| **Append-only** | Logs are never modified or deleted; immutable disk segments |
| **Bounded resources** | Fixed-capacity buffers; memory capped at 250MB via segment flushing |
| **Decoupled I/O** | Network threads never block on indexing; workers never block on network |
| **Thread safety by design** | Immutable segments, atomic counters, minimal lock scope |
| **Simplicity first** | Correct code before fast code; optimize only after measuring |

---

## System Phases

### ✅ Phase 1 — Core Data Structures *(Complete)*

The foundation: tokenization, inverted indexing, and in-memory search primitives.

**Components:**
- **Tokenizer** — Converts raw log text into normalized, searchable terms
  - Lowercase normalization for case-insensitive search
  - Stop-word filtering (50+ common English words removed)
  - Character-by-character parsing for O(n) complexity
  - Handles punctuation, numbers, and mixed case
- **InvertedIndex** — Maps terms to sorted posting lists of document IDs
  - `unordered_map<string, vector<DocID>>` for O(1) term lookup
  - Monotonically increasing DocIDs keep posting lists sorted automatically
  - Thread-safe insertion via mutex + atomics
  - Memory usage tracking for future segment flushing
  - Duplicate prevention when terms appear multiple times in one document

**Key design decisions:**
- `vector` for posting lists (cache-friendly, O(1) amortized append)
- Monotonic DocIDs so insertion order matches sorted order
- Tokenization performed outside the lock for parallelism

---

### ✅ Phase 2 — Ingestion Pipeline *(Complete)*

The network layer: high-throughput ingestion from TCP clients to the index.

**Components:**
- **TCPServer** — Accepts log connections and reads newline-delimited JSON
  - Thread-per-connection model
  - Handles partial reads across `recv()` boundaries
  - Graceful shutdown via `shutdown()` + `close()` sequence
- **RingBuffer<T>** — Thread-safe bounded queue
  - Template-based for reusability
  - Blocking (`push`/`pop`) and non-blocking (`try_push`/`try_pop`) operations
  - Timed `pop_for()` for efficient shutdown signaling
  - Separate condition variables for producers and consumers
  - Internal unlocked helpers prevent recursive mutex deadlock
- **WorkerPool** — Parallel log processing
  - Configurable worker count (defaults to `hardware_concurrency`)
  - Each worker has its own `LogParser` (zero contention)
  - Drains buffer on shutdown — no logs lost
  - Tracks processed count via atomics
- **LogParser** — Zero-dependency JSON extraction
  - Minimal parser for the known log format
  - Returns `std::optional<LogDocument>` on success/failure
  - Handles fields in any order

**Key design decisions:**
- Bounded buffer provides natural backpressure
- `try_push()` with timeout allows clean shutdown
- `shutdown()` before `close()` reliably interrupts blocked `accept()`/`recv()`
- Mutex released before joining threads to prevent lock-order deadlock

**Performance achieved:**
| Test | Clients | Duration | Throughput | Total Logs |
|------|---------|----------|------------|------------|
| Quick | 4 | 10 sec | 104,025 logs/sec | 1,058,555 |
| Extended | 8 | 31 sec | 99,644 logs/sec | 3,069,247 |

4.2x faster than the 25,000 logs/sec target.

---

### 🔄 Phase 3 — Storage & Persistence *(In Progress)*

Extending the index beyond memory: immutable disk segments and LSM-style flushing.

**Planned components:**
- **Segment format** — Immutable binary files containing serialized posting lists and documents
- **FlushWorker** — Background thread that writes the memory index to disk when it exceeds 64MB
- **SegmentManager** — Tracks multiple disk segments and coordinates queries across them
- **Write-Ahead Log (WAL)** — Crash recovery for in-flight logs
- **Search across memory + disk** — Merge results from the active memory index and all disk segments

**Design rationale:**
- Immutable segments eliminate read-write conflicts (no locks on reads)
- Sequential disk writes are dramatically faster than random writes
- LSM-style architecture is how Cassandra, RocksDB, and Lucene work

---

### ⏳ Phase 4 — Query Engine *(Planned)*

The search interface: from raw queries to ranked results.

**Planned components:**
- **QueryParser** — Parses search strings like `"ERROR" AND "database"` into an AST
- **QueryExecutor** — Executes boolean operations across memory and disk posting lists
- **Boolean operations** — Two-pointer intersection (AND), union (OR), difference (NOT)
- **Time-range filtering** — Restrict results by timestamp
- **Result merging** — Combine and sort results from multiple segments

**Design rationale:**
- Two-pointer intersection is O(A + B) instead of O(A × B)
- Skip lists (future optimization) for faster intersection on large posting lists

---

## Key Technical Decisions & Trade-offs

### Inverted Index vs. Grep Scan

We trade write-time tokenization CPU for O(K) read lookup time instead of O(N) full-text disk scan. Building the index costs more upfront but makes every subsequent query dramatically faster.

### LSM-Style Segments vs. In-Place Updates

Logs are append-only by nature. Immutable segment flushes convert random disk writes into fast sequential writes. Once written, segments are never modified — eliminating read-write lock contention entirely.

### Sorted Posting Lists

Posting lists are strictly sorted by DocID to enable O(A + B) linear-time intersections for multi-term boolean queries. Monotonic DocIDs mean new entries always append to the end, preserving sort order for free.

### Bounded Ring Buffer

A fixed-capacity buffer prevents unbounded memory growth and provides natural backpressure. When the buffer is full, producers block (or drop, depending on configuration) rather than exhausting memory.

### Thread-per-Connection vs. Event Loop

For the current throughput target, the thread-per-connection model is simpler and easier to reason about. Event-loop architectures (epoll, kqueue) are planned as future work for handling tens of thousands of concurrent connections.

---

## Project Structure

```
log-aggregator/
├── include/                  # Public headers
│   ├── types.hpp             # Core data structures (LogDocument, DocID, etc.)
│   ├── tokenizer.hpp         # Text tokenization
│   ├── index.hpp             # Inverted index
│   ├── ring_buffer.hpp       # Thread-safe bounded queue (header-only)
│   ├── log_parser.hpp        # JSON parsing
│   ├── worker_pool.hpp       # Parallel processing
│   └── tcp_server.hpp        # Network ingestion
├── src/                      # Implementation files
│   ├── tokenizer.cpp
│   ├── index.cpp
│   ├── log_parser.cpp
│   ├── worker_pool.cpp
│   ├── tcp_server.cpp
│   └── main.cpp
├── tests/                    # Test suites
│   ├── test_tokenizer.cpp
│   ├── test_index.cpp
│   ├── test_ring_buffer.cpp
│   ├── test_tcp_server.cpp
│   └── test_integration.cpp
├── tools/                    # Utilities
│   ├── benchmark.cpp         # Performance testing
│   └── log_generator.cpp     # Synthetic load generation
├── CMakeLists.txt
└── README.md
```

---

## Technology Stack

| Component | Choice | Why |
|-----------|--------|-----|
| Language | C++17 | Performance, low-level control, zero runtime overhead |
| Build system | CMake | Cross-platform, industry standard |
| Threading | `std::thread`, `std::mutex`, `std::condition_variable` | Standard library — no dependencies |
| Networking | POSIX sockets | Direct, no abstraction overhead |
| Testing | Assert-based + custom harness | No external framework needed |
| Storage | Custom binary format (Phase 3) | Full control over layout |

---

## Testing Strategy

Tests are organized by component and include:
- **Unit tests** — Individual components in isolation
- **Integration tests** — Full pipeline: TCP → Buffer → Workers → Index
- **Regression tests** — Documented deadlock scenarios
- **Performance tests** — Throughput and latency benchmarks

Each test uses explicit timeouts to catch deadlocks rather than hanging indefinitely.

---

## What I Learned

This project has been a deep dive into systems programming:

- **Concurrency** — Mutexes, condition variables, atomics, deadlock diagnosis
- **Networking** — Socket lifecycle, partial reads, graceful shutdown with `shutdown()` vs `close()`
- **Data structures** — Inverted indexes, posting lists, ring buffers, two-pointer intersection
- **Performance** — Benchmarking, profiling, identifying bottlenecks
- **Debugging** — Finding and fixing a real deadlock caused by `close()` not interrupting `accept()`

---

## Roadmap

- [x] **Phase 1** — Core data structures (Tokenizer, InvertedIndex)
- [x] **Phase 2** — Ingestion pipeline (TCP, RingBuffer, Workers)
- [ ] **Phase 3** — Storage & persistence (segments, flushing, WAL)
- [ ] **Phase 4** — Query engine (parser, boolean operations, time-range)
- [ ] **Phase 5** — Advanced features (HTTP API, web dashboard)
- [ ] **Phase 6** — Production hardening (TLS, auth, monitoring)

---

## License

MIT License — free to use for learning or as a reference.

---

**Built from scratch in C++17. Zero dependencies. 100K+ logs/sec.**