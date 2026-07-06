# 1. Concurrency Model

Date: 2026-07-06 (retroactive)

## Status

Accepted

## Context

The HTTP/1.0 server must handle multiple concurrent client connections efficiently. Creating a new operating system thread for every incoming connection causes high memory overhead and context-switching delays under heavy load, and severely limits vertical scalability.

## Decision

We will implement a single threaded event based concurrency model using a task queue. We will use the `epoll` I/O multiplexing API to monitor multiple connections for readiness.

## Consequences

*   **Positive:** Limits maximum resource consumption and prevents system exhaustion during traffic spikes.
*   **Positive:** Greatly increases the number of concurrent connections that can be handled per server instance.
*   **Positive:** `epoll` provides O(1) time complexity for monitoring connections in contrast to O(n) for regular polling approaches.
*   **Negative:** Requires complex synchronization of the event loop and using `epoll` limits cross-platform compatibility.
*   **Negative:** Horizontal scalability requires load balancing proxies and IPC synchronization between multiple server instances.