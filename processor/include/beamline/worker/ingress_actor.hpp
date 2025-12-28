#pragma once

#include <caf/all.hpp>
#include "beamline/worker/core.hpp"
#include "beamline/worker/actors.hpp"

namespace beamline::worker {

class IngressActorState {
public:
    IngressActorState(caf::scheduled_actor* self, const std::string& nats_url, caf::actor worker);
    
    caf::behavior make_behavior();

private:
    caf::scheduled_actor* self_;
    std::string nats_url_;
    caf::actor worker_;
};

class IngressActor : public caf::event_based_actor {
public:
    IngressActor(caf::actor_config& cfg, std::string nats_url, caf::actor worker)
        : caf::event_based_actor(cfg),
          state_(this, std::move(nats_url), std::move(worker)) {}

    caf::behavior make_behavior() override {
        return state_.make_behavior();
    }

private:
    IngressActorState state_;
};

using ingress_actor = IngressActor;

} // namespace beamline::worker
