#include "detail/transform.hpp"

#include "detail/rect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ffpsd::detail
{
    Transform Transform::Shift(double dx, double dy) noexcept
    {
        return {1, 0, 0, 1, dx, dy};
    }

    Transform Transform::Scale(double sx, double sy, double origin_x, double origin_y) noexcept
    {
        return {sx, 0, 0, sy, origin_x * (1 - sx), origin_y * (1 - sy)};
    }

    Transform Transform::Rotate(Rotation rotation, const Rect& frame) noexcept
    {
        const auto width = static_cast<double>(frame.GetWidth());
        const auto height = static_cast<double>(frame.GetHeight());
        Transform turn;
        switch (rotation)
        {
        case Rotation::k90:
            turn = {0, 1, -1, 0, height, 0};
            break;
        case Rotation::k180:
            turn = {-1, 0, 0, -1, width, height};
            break;
        default:
            turn = {0, -1, 1, 0, 0, width};
            break;
        }
        return Shift(-frame.left, -frame.top).Then(turn).Then(Shift(frame.left, frame.top));
    }

    Transform Transform::Flip(FlipDirection direction, const Rect& frame) noexcept
    {
        const auto width = static_cast<double>(frame.GetWidth());
        const auto height = static_cast<double>(frame.GetHeight());
        const Transform mirror =
            direction == FlipDirection::kHorizontal ? Transform{-1, 0, 0, 1, width, 0} : Transform{1, 0, 0, -1, 0, height};
        return Shift(-frame.left, -frame.top).Then(mirror).Then(Shift(frame.left, frame.top));
    }

    Transform Transform::Then(const Transform& next) const noexcept
    {
        return {
            next.a * a + next.c * b,
            next.b * a + next.d * b,
            next.a * c + next.c * d,
            next.b * c + next.d * d,
            next.a * tx + next.c * ty + next.tx,
            next.b * tx + next.d * ty + next.ty};
    }

    Rect Transform::Apply(const Rect& rect) const
    {
        const auto x = [&](double px, double py) { return a * px + c * py + tx; };
        const auto y = [&](double px, double py) { return b * px + d * py + ty; };
        const double x1 = x(rect.left, rect.top);
        const double x2 = x(rect.right, rect.bottom);
        const double y1 = y(rect.left, rect.top);
        const double y2 = y(rect.right, rect.bottom);

        const std::int64_t left = std::llround(std::min(x1, x2));
        const std::int64_t top = std::llround(std::min(y1, y2));
        std::int64_t right = std::llround(std::max(x1, x2));
        std::int64_t bottom = std::llround(std::max(y1, y2));
        if (rect.GetWidth() > 0 && rect.GetHeight() > 0)
        {
            right = std::max(right, left + 1);
            bottom = std::max(bottom, top + 1);
        }
        return CheckedRect(top, left, bottom, right);
    }
} // namespace ffpsd::detail
