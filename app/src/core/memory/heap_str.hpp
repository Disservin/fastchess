#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace fastchess::util {

template <typename CharT, typename Traits = std::char_traits<CharT>>
class heap_str {
   private:
    std::unique_ptr<CharT[]> data_;
    size_t size_     = 0;
    size_t capacity_ = 0;

    static constexpr size_t min_capacity = 8;

    void reallocate(size_t new_capacity) {
        new_capacity  = std::max(new_capacity, min_capacity);
        auto new_data = std::make_unique<CharT[]>(new_capacity);

        if (data_) {
            Traits::copy(new_data.get(), data_.get(), size_);
        }

        data_     = std::move(new_data);
        capacity_ = new_capacity;
    }

   public:
    // Constructors
    heap_str() = default;

    heap_str(const CharT* str) {
        if (!str) return;

        size_ = Traits::length(str);
        if (size_ > 0) {
            reallocate(size_ + 1);
            Traits::copy(data_.get(), str, size_);
            data_[size_] = CharT();
        }
    }

    heap_str(const heap_str& other) {
        if (other.size_ > 0) {
            reallocate(other.size_ + 1);
            Traits::copy(data_.get(), other.data_.get(), other.size_);
            size_        = other.size_;
            data_[size_] = CharT();
        }
    }

    heap_str(heap_str&& other) noexcept
        : data_(std::move(other.data_)), size_(other.size_), capacity_(other.capacity_) {
        other.size_     = 0;
        other.capacity_ = 0;
    }

    // Assignment operators
    heap_str& operator=(const heap_str& other) {
        if (this != &other) {
            heap_str temp(other);
            swap(temp);
        }
        return *this;
    }

    heap_str& operator=(heap_str&& other) noexcept {
        if (this != &other) {
            data_           = std::move(other.data_);
            size_           = other.size_;
            capacity_       = other.capacity_;
            other.size_     = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // Element access
    CharT& operator[](size_t pos) { return data_[pos]; }
    const CharT& operator[](size_t pos) const { return data_[pos]; }

    CharT& at(size_t pos) {
        if (pos >= size_) throw std::out_of_range("Index out of range");
        return data_[pos];
    }

    const CharT& at(size_t pos) const {
        if (pos >= size_) throw std::out_of_range("Index out of range");
        return data_[pos];
    }

    // Capacity
    bool empty() const noexcept { return size_ == 0; }
    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return capacity_; }

    void reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            reallocate(new_capacity);
        }
    }

    void shrink_to_fit() {
        if (size_ + 1 < capacity_) {
            reallocate(size_ + 1);
        }
    }

    // Modifiers
    void clear() noexcept {
        size_ = 0;
        if (data_) data_[0] = CharT();
    }

    void push_back(CharT ch) {
        if (size_ + 1 >= capacity_) {
            reallocate(capacity_ * 2 + 2);
        }
        data_[size_] = ch;
        size_++;
        data_[size_] = CharT();
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
            data_[size_] = CharT();
        }
    }

    heap_str& append(const heap_str& str) { return append(str.data_.get(), str.size_); }

    heap_str& append(const CharT* str, size_t count) {
        if (count == 0) return *this;
        size_t new_size = size_ + count;
        if (new_size >= capacity_) {
            reallocate(new_size + 1);
        }
        Traits::copy(data_.get() + size_, str, count);
        size_        = new_size;
        data_[size_] = CharT();
        return *this;
    }

    // String operations
    const CharT* c_str() const noexcept { return data_ ? data_.get() : &empty_string; }

    const CharT* data() const noexcept { return data_ ? data_.get() : &empty_string; }

    void swap(heap_str& other) noexcept {
        data_.swap(other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

   private:
    static const CharT empty_string;
};

template <typename CharT, typename Traits>
const CharT heap_str<CharT, Traits>::empty_string = CharT();

// Non-member functions
template <typename CharT, typename Traits>
bool operator==(const heap_str<CharT, Traits>& lhs, const heap_str<CharT, Traits>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    return Traits::compare(lhs.c_str(), rhs.c_str(), lhs.size()) == 0;
}

template <typename CharT, typename Traits>
bool operator!=(const heap_str<CharT, Traits>& lhs, const heap_str<CharT, Traits>& rhs) {
    return !(lhs == rhs);
}

template <typename CharT, typename Traits>
heap_str<CharT, Traits> operator+(const heap_str<CharT, Traits>& lhs, const heap_str<CharT, Traits>& rhs) {
    heap_str<CharT, Traits> result(lhs);
    result.append(rhs);
    return result;
}

template <typename CharT, typename Traits>
bool operator==(const heap_str<CharT, Traits>& lhs, const CharT* rhs) {
    return Traits::compare(lhs.c_str(), rhs, lhs.size()) == 0;
}

using heap_string = heap_str<char>;
}  // namespace fastchess::util
