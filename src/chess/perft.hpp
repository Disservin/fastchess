#include "board.hpp"
#include "movegen.hpp"

namespace fast_chess
{
class Perft
{
  private:
    uint64_t nodes = 0;

  public:
    bool print = false;

    uint64_t perftFunction(Board &b, int depth, int max);

    uint64_t getAndResetNodes();
};

} // namespace fast_chess
