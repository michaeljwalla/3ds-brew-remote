#pragma once

#include <cmath>
#include <ostream>
#include <type_traits>
#include "logger.h"

template<typename T = float>
struct coords {
    T x, y;

    coords(): x{0}, y{0} {}
    coords(T x, T y): x{x}, y{y} {}

    template<typename U>
    explicit coords(const coords<U>& other)
        : x{static_cast<T>(other.x)}, y{static_cast<T>(other.y)} {}

    template<typename U>
    coords<std::common_type_t<T, U>> operator+(const coords<U>& other) const {
        using R = std::common_type_t<T, U>;
        return { static_cast<R>(x) + static_cast<R>(other.x),
                 static_cast<R>(y) + static_cast<R>(other.y) };
    }

    template<typename U>
    coords& operator+=(const coords<U>& other) {
        x = static_cast<T>(x + other.x);
        y = static_cast<T>(y + other.y);
        return *this;
    }

    template<typename U>
    coords<std::common_type_t<T, U>> operator-(const coords<U>& other) const {
        using R = std::common_type_t<T, U>;
        return { static_cast<R>(x) - static_cast<R>(other.x),
                 static_cast<R>(y) - static_cast<R>(other.y) };
    }

    template<typename U>
    coords& operator-=(const coords<U>& other) {
        x = static_cast<T>(x - other.x);
        y = static_cast<T>(y - other.y);
        return *this;
    }

    template<typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    coords<std::common_type_t<T, U>> operator*(U scalar) const {
        using R = std::common_type_t<T, U>;
        return { static_cast<R>(x) * static_cast<R>(scalar),
                 static_cast<R>(y) * static_cast<R>(scalar) };
    }

    template<typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    coords& operator*=(U scalar) {
        x = static_cast<T>(x * scalar);
        y = static_cast<T>(y * scalar);
        return *this;
    }

    template<typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    coords<std::common_type_t<T, U>> operator/(U scalar) const {
        using R = std::common_type_t<T, U>;
        return { static_cast<R>(x) / static_cast<R>(scalar),
                 static_cast<R>(y) / static_cast<R>(scalar) };
    }

    template<typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    coords& operator/=(U scalar) {
        x = static_cast<T>(x / scalar);
        y = static_cast<T>(y / scalar);
        return *this;
    }

    //not a dot/cross! multiplies components individually
    template<typename U>
    coords<std::common_type_t<T, U>> operator*(const coords<U>& other) const {
        using R = std::common_type_t<T, U>;
        return { static_cast<R>(x) * static_cast<R>(other.x),
                 static_cast<R>(y) * static_cast<R>(other.y) };
    }

    template<typename U>
    coords& operator*=(const coords<U>& other) {
        x = static_cast<T>(x * other.x);
        y = static_cast<T>(y * other.y);
        return *this;
    }

    auto magnitude() const {
        using R = std::common_type_t<T, float>;
        return std::sqrt(static_cast<R>(x) * x + static_cast<R>(y) * y);
    }
    auto normalized() const {
        using R = std::common_type_t<T, float>;
        auto mag = this->magnitude();
        if (mag == 0) return coords<R>{0, 0};
        return *this / mag;
    }

};
template<typename T>
inline std::ostream& operator<<(std::ostream& out, const coords<T>& c) {
    out << "[" << c.x << ", " << c.y << "]";
    return out;
}
template<typename T>
inline Logger& operator<<(Logger& out, const coords<T>& c) {
    out << "[" << c.x << ", " << c.y << "]";
    return out;
}
template<typename T, typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
coords<std::common_type_t<T, U>> operator*(U scalar, const coords<T>& c) {
    return c * scalar;
}
