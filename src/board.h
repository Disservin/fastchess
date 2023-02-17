#pragma once

class Board
{
  public:
    Board();
    Board(Board &&) = default;
    Board(const Board &) = default;
    Board &operator=(Board &&) = default;
    Board &operator=(const Board &) = default;
    ~Board();

  private:
};

Board::Board()
{
}

Board::~Board()
{
}