#pragma once

#include <cmath>

template<typename T = float>
struct coords {
    T x, y;

    coords(): x{0}, y{0} {}
    coords(T x, T y): x{x}, y{y} {}

    coords operator+(const coords& other) const {
        return { x + other.x, y + other.y };
    }

    coords& operator+=(const coords& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    coords operator-(const coords& other) const {
        return { static_cast<T>(x - other.x), static_cast<T>(y - other.y) };
    }

    coords& operator-=(const coords& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    coords operator*(T scalar) const {
        return { x * scalar, y * scalar };
    }

    coords& operator*=(T scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    coords operator/(T scalar) const {
        return { x / scalar, y / scalar };
    }

    coords& operator/=(T scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    T magnitude() const {
        return static_cast<T>(std::sqrt(x * x + y * y));
    }
};
