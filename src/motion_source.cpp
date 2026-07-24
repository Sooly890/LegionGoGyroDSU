#include "motion_source.hpp"

#include <iostream>

namespace motion
{
auto Vec3::operator[](int index) -> double&
{
    if (index == 1)
        return y;
    if (index == 2)
        return z;
    if (index != 0)
        std::cerr << "Invalid Vec3 index " << index << '\n';
    return x;
}

auto Vec3::operator[](int index) const -> const double&
{
    if (index == 1)
        return y;
    if (index == 2)
        return z;
    if (index != 0)
        std::cerr << "Invalid Vec3 index " << index << '\n';
    return x;
}

auto Vec3::operator+(const Vec3& other) const -> Vec3
{
    return {x + other.x, y + other.y, z + other.z};
}

auto Vec3::operator-(const Vec3& other) const -> Vec3
{
    return {x - other.x, y - other.y, z - other.z};
}

auto Vec3::operator*(const Vec3& other) const -> Vec3
{
    return {x * other.x, y * other.y, z * other.z};
}

auto Vec3::operator*(double scalar) const -> Vec3
{
    return {x * scalar, y * scalar, z * scalar};
}

auto Vec3::operator/(double scalar) const -> Vec3
{
    return {x / scalar, y / scalar, z / scalar};
}

void Vec3::operator+=(const Vec3& other)
{
    x += other.x;
    y += other.y;
    z += other.z;
}
} // namespace motion
