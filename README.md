# PROJECT-
Design Decisions

Scheduler: SJF Non-preemptive
Why SJF?
Shortest Job First minimises average turnaround time — provably optimal for non-preemptive single-queue scheduling. With the given workload (burst times 1–9), SJF front-loads short tasks (T8 burst=1, T4 burst=2, T14 burst=2) so cores are rarely starved. Average turnaround achieved: 4 cycles across all 15 tasks.
Trade-off: a very long task (T7 burst=9) can be delayed, but that is acceptable here since all tasks arrive at time 0.
Multi-core: 2 parallel cores
Both cores draw from a single shared SJF ready-queue. Whichever core is free next picks the shortest remaining job. This gives ~2× throughput over a single core: 15 tasks in 37 cycles vs the ~74 cycles a single-core run would need.
Cache: L1/L2/L3 with LRU eviction
LRU (Least Recently Used) beats FIFO for real workloads because repeatedly accessed blocks (e.g. M1 is needed by T1, T2, T3, T7, T10, T13) stay hot in cache.
Three-level hierarchy: L1(32), L2(128), L3(512).
On a hit: block is touched (moved to back of LRU queue) and the level's latency is charged.
On a miss: next level is searched. On a RAM fetch, the block is promoted into L1, and any displaced block cascades to L2 → L3 → discarded.
