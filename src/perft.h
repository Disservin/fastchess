#include <iostream>

#include "board.h"
#include "movegen.h"

class Perft
{
  private:
    uint64_t nodes = 0;

  public:
    bool print = false;

    uint64_t perftFunction(Board &b, int depth, int max);

    uint64_t getAndResetNodes();
};
