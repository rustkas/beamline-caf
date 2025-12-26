# CAF Worker Architecture (CP1/CP2 boundary aware) 
 
 ## System context (stack) 
 - Router (Erlang/OTP) — policy evaluation + orchestration 
 - NATS/JetStream — transport / persistence 
 - CAF Worker (C++/CAF) — execution engine 
 - ui_web — UI (later) 
 
 ## Core message flow (CP1) 
 1) Router → NATS: publish ExecAssignment to `caf.exec.assign.v1` 
 2) CAF Worker ← NATS: subscribe `caf.exec.assign.v1` 
 3) CAF Worker → NATS: publish ack `caf.exec.assign.v1.ack` 
 4) CAF Worker executes job (block executor) 
 5) CAF Worker → NATS: publish result `caf.exec.result.v1` 
 6) Router ← NATS: consumes results and continues flow 
 
 ## Subjects (CP1 baseline) 
 - `caf.exec.assign.v1` — assignments (Router → Worker) 
 - `caf.exec.assign.v1.ack` — assignment ack (Worker → Router) 
 - `caf.exec.result.v1` — results (Worker → Router) 
 - `caf.exec.dlq.v1` — DLQ envelope (if enabled) 
 
 ## Envelope expectations (contract-level) 
 The exact encoding MUST be verified from code (protobuf vs JSON). 
 Docs show JSON examples, but **implementation is authoritative**. 
 
 Minimum fields to verify end-to-end: 
 - correlation identifiers: `assignment_id`, `request_id` (or equivalents) 
 - tenancy: `tenant_id` 
 - tracing: `trace_id` 
 - timestamps (ms) if present: `acknowledged_at`, `completed_at` 
 - status enums: accepted/success/error/timeout/etc. 
 
 ## CP1 vs CP2 
 - CP1: frozen Router behavior. CAF validation must not require Router changes. 
 - CP2: may introduce stricter header/version rules, but only after evidence. 
 
 ## Local evidence rule 
 Every architectural claim here must be backed by: 
 - code references (file + line), 
 - runtime evidence (logs/artifacts), 
 - or a reproducible command captured in a task `progress.md`. 
