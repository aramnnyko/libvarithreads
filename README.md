
# libvarithreads

> Configurable user-level threading library. Pre-alpha. Work in progress.

_Read this in: **English** | [Русский](README.ru.md)_

One codebase, many configurations — selected at compile time.

---

## Why

A way to fine-tune the execution mode and scheduler for a task without
jumping between projects. One codebase, compile-time selection: you build
exactly the configuration your workload needs, with no runtime dispatch
and no unused code.

## Status

Currently at the **simulator stage**: the **DRRD** scheduler
(Deficit Round Robin with dynamic weights) is being verified in a
standalone simulator. No runtime yet.

See [ALGORITHMS_DRRD.md](ALGORITHMS_DRRD.md) for the scheduler design.

## Planned configuration axes

| Axis | Options |
|------|---------|
| Execution mode | `COOPERATIVE` / `PREEMPTIVE` / `HYBRID` |
| Scheduler | `DRRD`; others TBD |
| Platform | x86-64 / Linux (planned); others TBD |

Names and mechanisms are subject to change.

### Scheduler: DRRD

DRRD is a Deficit Round Robin variant with dynamic weights. The set of
threads can change mid-cycle; shares are recomputed on the fly. The
guarantee is given at the level of individual threads, not just groups.
In the long run: over a full cycle without blocking, a thread receives
exactly `priority` quanta; in cycles with blocking, the shortfall becomes
debt and is compensated later.

Key properties:

- **Proportional sharing** — over a full cycle, each thread receives a
  number of quanta equal to its priority. A thread with priority 3 gets
  3 quanta per cycle. Threads within a group share the same priority
  (a group is an equivalence class by priority), so the group's weight
  is `priority × count`, and within the group threads share it equally.
- **Dynamic weights** — threads can be added and removed mid-cycle,
  without stopping the scheduler, while preserving the proportional
  sharing guarantee.
- **Debt tracking** — if a thread is blocked, its scheduling decision is
  spent but no quantum is delivered. The thread records this as debt. On
  unblock, it receives the deferred quantum: two quanta per decision
  instead of one. Other threads receive their usual number of decisions;
  debt does not reduce their share in the long run.
- **O(G) per operation** — cost depends on the number of groups `G`,
  which is bounded by 10 by design, so effectively constant.

#### When it fits

- Mixed workloads where different classes of threads need different
  shares of CPU time.
- Systems with threads that block and unblock frequently — the debt
  mechanism preserves long-term fairness.
- Scenarios with a dynamic thread set: servers, task pools, where
  threads are constantly created and finished.
- Embedded systems with a small number of priority levels
  (architecturally up to 10 groups).

#### When it does not fit

- **Hard real-time systems with deadlines.** DRRD provides a proportional
  share but no upper bound on latency or jitter. Deadlines require EDF
  or a similar scheduler.
- **Many priority levels.** The algorithm is designed for ≤10 groups;
  with more priorities, O(G) stops being constant.
- **Frequent priority changes.** Changing priority means removing a
  thread from one group and adding it to another. This resets the
  thread's accumulated debt and forces a recomputation of all groups'
  deficits. Frequent changes increase overhead.
- **Multi-core systems.** DRRD is a single-core scheduler. Cross-core
  balancing is a separate problem, not its concern.

The algorithm is described in detail in [ALGORITHMS_DRRD.md](ALGORITHMS_DRRD.md).

## Repository layout

```
libvarithreads/
├── README.md              — this file (English)
├── README.ru.md           — Russian translation
├── ALGORITHMS_DRRD.md     — DRRD scheduler design
├── LICENSE                — Apache 2.0
├── NOTICE                 — attribution and ethical request
└── sim/                   — simulator (current focus)
```

## Roadmap

- [x] DRRD simulator
- [ ] Simulator verification
- [ ] Thread core (create / yield / join)
- [ ] x86-64 assembly context switch
- [ ] DRRD as a scheduler
- [ ] Compile-time configuration dispatcher
- [ ] Cooperative, preemptive, hybrid modes
- [ ] Additional schedulers
- [ ] Other architectures

**By October 31, 2026:** DRRD scheduler and cooperative, preemptive, hybrid
modes.

## License

Apache License 2.0 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).

The author asks that this project not be used for military purposes —
see [NOTICE](NOTICE) for details. This request is not part of the
license terms.

## Author

aramnnyko — [GitHub](https://github.com/aramnnyko)

## Contributing

The project is at an early stage. Bug reports and design discussions are
welcome; large pull requests are better discussed first.
