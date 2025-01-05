#pragma once

#include <utility>

namespace fastchess {

template <typename T, typename U>
struct GamePair {
    T white;
    U black;

    GamePair() = default;
    GamePair(T white, U black) : white(white), black(black) {}
    GamePair(const GamePair<T, U> &other) : white(other.white), black(other.black) {}
    GamePair(GamePair<T, U> &&other) noexcept : white(std::move(other.white)), black(std::move(other.black)) {}

    GamePair<T, U> &operator=(const GamePair<T, U> &other) {
        if (this != &other) {
            white = other.white;
            black = other.black;
        }

        return *this;
    }

    GamePair<T, U> &operator=(GamePair<T, U> &&other) noexcept {
        if (this != &other) {
            white = std::move(other.white);
            black = std::move(other.black);
        }

        return *this;
    }

    void swap(GamePair<T, U> &other) {
        std::swap(white, other.white);
        std::swap(black, other.black);
    }
};

}  // namespace fastchess