#include "../src/path_safety.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > PATH_MAX) return 0;
    const std::string candidate(reinterpret_cast<const char *>(data), size);
    if (sre::path::safe_relative_path(candidate)) {
        if (candidate.empty() || candidate.front() == '/' || candidate.size() >= PATH_MAX ||
            candidate.find('\0') != std::string::npos) std::abort();
        std::size_t start = 0;
        while (start < candidate.size()) {
            const std::size_t end = candidate.find('/', start);
            const std::string component = candidate.substr(
                start, end == std::string::npos ? end : end - start);
            if (component.empty() || component.size() > NAME_MAX ||
                component == "." || component == "..") std::abort();
            if (end == std::string::npos) break;
            start = end + 1U;
        }
    }
    if (sre::path::safe_snapshot_id(candidate)) {
        if (candidate.empty() || candidate.size() > 128U) std::abort();
        for (const unsigned char c : candidate)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_')) std::abort();
    }
    return 0;
}
