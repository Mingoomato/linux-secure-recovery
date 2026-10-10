#ifndef SRE_PATH_SAFETY_HPP
#define SRE_PATH_SAFETY_HPP

#include <climits>
#include <string_view>

namespace sre::path {

inline bool safe_snapshot_id(std::string_view id) {
    if (id.empty() || id.size() > 128U) return false;
    for (const unsigned char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

inline bool safe_relative_path(std::string_view path) {
    if (path.empty() || path.size() >= PATH_MAX || path.front() == '/' || path.back() == '/')
        return false;
    for (const unsigned char c : path)
        if (c == '\0') return false;

    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string_view component = path.substr(
            start, end == std::string_view::npos ? end : end - start);
        if (component.empty() || component.size() > NAME_MAX ||
            component == "." || component == "..") return false;
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return true;
}

} // namespace sre::path

#endif
