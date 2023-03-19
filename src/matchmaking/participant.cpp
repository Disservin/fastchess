#include "matchmaking/participant.hpp"
#include "participant.hpp"

namespace fast_chess
{

Participant::Participant(const EngineConfiguration &config)
{
    loadConfig(config);
    resetError();
    startEngine(config.cmd);
    checkErrors();

    info_.config = config;
}

} // namespace fast_chess
