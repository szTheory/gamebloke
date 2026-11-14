# Plan23 current hosted producer proof

[Hosted run](https://github.com/szTheory/gamebloke/actions/runs/43560267506) completed successfully at consumer HEAD `56c22133f3697f6a78fed6c0e1a549659707056b`. Immutable producer remains `85c361edfd6b56640e68e64c5e32527f85408836`, input `95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`. All eleven required jobs passed within their exact approved budgets. The promoted collector and committed final-hosted-proof validator accepted the actual run/jobs API data and four distinct log witnesses. Local r4 receipts and producer files are unchanged.

| Job | Seconds | Limit |
| --- | ---: | ---: |
| macOS native build |163 |300 |
| macOS ARM diagnostics |270 |300 |
| macOS Thumb diagnostics |155 |180 |
| Linux native build |114 |180 |
| Linux ARM diagnostics |92 |180 |
| Linux Thumb diagnostics |81 |180 |
| Sanitizers |100 |180 |
| Hygiene |105 |180 |
| Changes / title / required aggregator |8 /5 /5 |180 each |

The current bottleneck is macOS ARM: about20 seconds to checkout/tool setup,243 seconds in the workflow, and final cleanup. CTest reports153.69 seconds; final coverage alone37.29 seconds. The job has30 seconds of observed headroom. This is one measurement, not a runtime guarantee. Linux ARM was92 seconds under the unchanged180-second cap. Preserve source proof, inspect changed inputs and final-scale admitted cost before a later hosted attempt; do not add caches, concurrency or dependencies speculatively.

Raw actual API documents, four logs, collected receipt, validation result and exact metadata transaction remain ignored in `build/phase05-job-budgets/hosted-attempt1/`. The local proof remains `build/phase05-plan23-proof-r4/preflight/local-proof.json`:722 corresponding files literally compared between macOS arm64 and native Linux arm64, plus retained historical comparisons and both-host audits. Hosted equality is log-derived hash evidence, distinct from local literal comparisons. No ROM or toolchain binary was uploaded.

The single additional attempt is consumed (one of one), successful; historical exhausted ledgers/stops remain historical. No automatic further attempt is authorized by this renewal. Canonical fixture metadata now rebinds722 exact unchanged output hashes to this producer; production admitted execution, integration and independent review remain pending. Plan23 and Phase5 remain incomplete. Continue authorized local work; no owner action is needed now. The one-time merge-commit recommendation remains reserved for the explicit Phase5 owner boundary.
