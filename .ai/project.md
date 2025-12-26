# CAF Worker (apps/caf) — Project Context 
 
 ## What this component is 
 CAF Worker is the execution engine (C++/CAF) that processes assignments emitted by Router via NATS/JetStream. 
 
 Primary responsibility: 
 - Subscribe `caf.exec.assign.v1` 
 - Acknowledge via `caf.exec.assign.v1.ack` 
 - Execute the job (HTTP/FS/SQL/Human approval, etc.) 
 - Publish result via `caf.exec.result.v1` 
 - (Optional) publish non-retriable failures to DLQ subject(s) 
 
 ## Why this `.ai/` exists (component-local) 
 This folder is a filesystem task-management system for **CAF-only** work: 
 - Source of truth for CAF execution tasks is **`.ai/task_*/progress.md`** 
 - Chat is not a source of truth. 
 
 ## Inputs / Outputs (high level) 
 Inputs: 
 - NATS subject: `caf.exec.assign.v1` (ExecAssignment) 
 
 Outputs: 
 - `caf.exec.assign.v1.ack` (ExecAssignmentAck) 
 - `caf.exec.result.v1` (ExecResult) 
 - (Optional) `caf.exec.dlq.v1` or other DLQ subjects 
 
 ## Repository pointers (expected) 
 - CAF worker code: `processor/` (per docs) 
 - Tests (if present): `test/` or `processor/tests/` 
 - Runtime configs: `config/` (or root-level config) 
 
 If actual paths differ, record the real paths in: 
 - `.ai/architecture.md` (component map) 
 - `.ai/task_*/progress.md` (evidence + commands) 
 
 ## Operating rule 
 Do not “improve” contracts or semantics while validating. 
 First: validate reality, capture evidence, then decide CP2 changes.
