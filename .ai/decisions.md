# CAF Decisions & Policies 
 
 ## D-CAF-001 — CP1 Router is frozen 
 - Any mismatch between CAF and Router is treated as FACT to drive CP2 design. 
 - Do not patch Router in CP1 branch. 
 
 ## D-CAF-002 — Validation before refactor 
 - First deliverable is an evidence-based validation of: 
   - connectivity, 
   - subject usage, 
   - contract compliance, 
   - failure semantics. 
 
 ## D-CAF-003 — Evidence lives in progress.md 
 - A step is considered DONE only if written in: 
   `apps/caf/.ai/task_*/progress.md` 
 - Include command + result + artifact path. 
 
 ## D-CAF-004 — c-gateway is not a dependency for CAF validation 
 - Current c-gateway integration is blocked by a stub NATS client (tracked elsewhere). 
 - CAF validation will use direct NATS/Router flows. 
