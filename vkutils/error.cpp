#include "error.hpp"

#include <cstdarg>

namespace vkutils {
    Error::Error(char const* format, ...) {
        va_list args;
        va_start(args, format);

        char buff[1024]{};
        vsnprintf(buff, 1023, format, args);

        va_end(args);

        message = buff;
    }

    char const* Error::what() const noexcept {
        return message.c_str();
    }
}
