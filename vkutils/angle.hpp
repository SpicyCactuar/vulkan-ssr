#pragma once

namespace vkutils {
    template<typename Scalar>
    constexpr Scalar pi = Scalar(3.14159265358979323846);

    template<typename Scalar>
    constexpr Scalar deg_to_rad(Scalar degrees) {
        return degrees * pi<Scalar> / Scalar(180);
    }

    template<typename Scalar>
    constexpr Scalar rad_to_deg(Scalar radians) {
        return radians * Scalar(180) / pi<Scalar>;
    }

    template<typename Scalar>
    class Degrees;
    template<typename Scalar>
    class Radians;

    template<typename Scalar>
    class Degrees {
    public:
        constexpr explicit Degrees(Scalar value = Scalar(0)) noexcept
            : scalarValue(value) {
        }

        constexpr Degrees(const Degrees& other) noexcept
            : scalarValue(other.scalarValue) {
        }

        constexpr Degrees& operator=(const Degrees& other) noexcept {
            scalarValue = other.scalarValue;
            return *this;
        }

        constexpr Degrees(const Radians<Scalar>& radians) noexcept
            : scalarValue(rad_to_deg(radians.value())) {
        }

        constexpr Degrees& operator=(const Radians<Scalar>& radians) noexcept {
            scalarValue = rad_to_deg(radians.value());
            return *this;
        }

        constexpr Scalar value() const noexcept {
            return scalarValue;
        }

    private:
        Scalar scalarValue;
    };

    template<typename Scalar>
    class Radians {
    public:
        constexpr explicit Radians(Scalar value = Scalar(0)) noexcept
            : scalarValue(value) {
        }

        constexpr Radians(const Radians& other) noexcept
            : scalarValue(other.scalarValue) {
        }

        constexpr Radians& operator=(const Radians& other) noexcept {
            scalarValue = other.scalarValue;
            return *this;
        }

        constexpr Radians(const Degrees<Scalar>& degrees) noexcept
            : scalarValue(deg_to_rad(degrees.value())) {
        }

        constexpr Radians& operator=(const Degrees<Scalar>& degrees) noexcept {
            scalarValue = deg(degrees.value());
            return *this;
        }

        constexpr Scalar value() const noexcept {
            return scalarValue;
        }

    private:
        Scalar scalarValue;
    };


    using Degreesf = Degrees<float>;
    using Radiansf = Radians<float>;

    template<typename Scalar>
    constexpr Scalar to_degrees(Degrees<Scalar> value) noexcept {
        return value.value();
    }

    template<typename Scalar>
    constexpr Scalar to_radians(Radians<Scalar> value) noexcept {
        return value.value();
    }


    inline namespace literals {
        constexpr Radiansf operator "" _radf(const long double value) noexcept {
            return Radiansf(static_cast<float>(value));
        }

        constexpr Degreesf operator "" _degf(const long double value) noexcept {
            return Degreesf(static_cast<float>(value));
        }
    }
}
