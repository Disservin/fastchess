#include "matchmaking/participant.hpp"
#include "participant.hpp"

namespace fast_chess
{

Participant::Participant(const EngineConfiguration &config)
{
    engine_.loadConfig(config);
    engine_.resetError();
    engine_.startEngine();
    engine_.checkErrors();
}

} // namespace fast_chess
