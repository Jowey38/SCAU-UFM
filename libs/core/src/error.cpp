#include <core/error.hpp>

#include <sstream>

namespace scau::core {

void throw_assert_failure(std::string_view condition,
                          std::string_view message,
                          const char* file,
                          int line) {
    std::ostringstream os;
    os << "SCAU_ASSERT failed: " << condition
       << " | " << message
       << " | at " << file << ':' << line;
    throw ScauError(os.str());
}

}  // namespace scau::core
