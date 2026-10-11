#ifndef FFPSD_DETAIL_TRANSFORM_HPP_
#define FFPSD_DETAIL_TRANSFORM_HPP_

#include <ffpsd/types.hpp>

namespace ffpsd::detail
{
    // x' = a·x + c·y + tx, y' = b·x + d·y + ty; only scales, mirrors and quarter turns, which keep rectangles upright.
    struct Transform
    {
        double a = 1;
        double b = 0;
        double c = 0;
        double d = 1;
        double tx = 0;
        double ty = 0;

        static Transform Shift(double dx, double dy) noexcept;
        static Transform Scale(double sx, double sy, double origin_x, double origin_y) noexcept;

        // The frame turns in place: its top left corner stays.
        static Transform Rotate(Rotation rotation, const Rect& frame) noexcept;
        static Transform Flip(FlipDirection direction, const Rect& frame) noexcept;

        Transform Then(const Transform& next) const noexcept;

        // Anything with pixels keeps one at least.
        Rect Apply(const Rect& rect) const;

        bool SwapsAxes() const noexcept
        {
            return a == 0;
        }
        bool IsShift() const noexcept
        {
            return a == 1 && b == 0 && c == 0 && d == 1;
        }
    };
} // namespace ffpsd::detail

#endif // FFPSD_DETAIL_TRANSFORM_HPP_
