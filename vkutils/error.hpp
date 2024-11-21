#pragma once

#include <string>
#include <exception>

namespace vkutils {
    /*
     * Class used for exceptions. Unlike e.g. std::runtime_error, which only
     * accepts a "fixed" string, Error provides std::printf()-like formatting.
     * Example:
     *
     *      throw Error("vkCreateInstance() returned %s", to_string(result).c_str());
     */
    class Error : public std::exception {
    public:
        explicit Error(char const*, ...);

        char const* what() const noexcept override;

    private:
        std::string message;
    };
}
