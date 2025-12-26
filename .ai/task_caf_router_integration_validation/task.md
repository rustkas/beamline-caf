# T-CAF-ROUTER-01 — CAF ↔ Router Integration Validation (Live) 
 
 ## Goal 
 Verify that CAF Worker has a REAL NATS/JetStream integration and that runtime interaction with Router is correct: 
 - subjects are used as expected, 
 - encoding matches contract (protobuf/JSON), 
 - ack/result semantics are correct, 
 - failures behave predictably. 
 
 ## Why now 
 We are moving down the stack after c-gateway was found blocked (stub NATS client). 
 Next production-critical integration is Router ↔ CAF. 
 
 ## Output 
 A PASS/FAIL decision with evidence: 
 - commands used, 
 - logs/artifacts collected, 
 - exact code references for subject usage and encoding.
