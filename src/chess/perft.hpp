#include "board.hpp"
#include "movegen.hpp"

namespace fast_chess
{
class Perft
{
  private:
    uint64_t nodes_ = 0;

  public:
    bool print_ = false;

    uint64_t perftFunction(Board &b, int depth, int max);

    uint64_t getAndResetNodes();
};

} // namespace fast_chess
