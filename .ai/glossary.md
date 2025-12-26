# CAF Glossary 
 
 - CAF: C++ Actor Framework 
 - Assignment: Execution request emitted by Router (ExecAssignment) 
 - Ack: Immediate acceptance/receipt signal for assignment (ExecAssignmentAck) 
 - Result: Completion output for assignment (ExecResult) 
 - DLQ: Dead Letter Queue — non-retriable failures routing 
 - TTG: Time To Green — time until system returns to operational state 
 - Subject: NATS topic name (e.g. `caf.exec.assign.v1`) 
 - JetStream: NATS persistence/streaming layer (consumers, durable, ack policy) 
