#include "beamline/worker/ingress_actor.hpp"
#include <iostream>

namespace beamline::worker {

IngressActorState::IngressActorState(caf::scheduled_actor* self, const std::string& nats_url, caf::actor worker)
    : self_(self), nats_url_(nats_url), worker_(worker) {
    // TODO: Connect to NATS
    std::cout << "IngressActor initialized with NATS URL: " << nats_url_ << std::endl;
    (void)self_; // Suppress unused warning
}

caf::behavior IngressActorState::make_behavior() {
    return {
        [](caf::atom_value atom) {
            if (atom == caf::atom("tick")) {
                 // Polling or similar
            }
        },
        // Handle incoming messages to be forwarded to worker
        [this](const std::string& json_request) {
            // Parse JSON and forward to worker
            // For now, just a stub
            std::cout << "Ingress received: " << json_request << std::endl;
            
            // To suppress unused warning for now:
            (void)worker_;
        }
    };
}

} // namespace beamline::worker
