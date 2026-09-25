// Minimal host-only Arduino String stand-in for the ThemeFileService runtime test.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
#include <cctype>
#define FILE_READ "r"
class String {
    std::string value_;
public:
    String() {}
    String(const char *s): value_(s ? s : "") {}
    String(const std::string &s): value_(s) {}
    size_t length() const { return value_.size(); }
    const char *c_str() const { return value_.c_str(); }
    char operator[](size_t i) const { return i < value_.size() ? value_[i] : 0; }
    int indexOf(const char *s) const { auto p = value_.find(s ? s : ""); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c) const { auto p = value_.find(c); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(char c) const { auto p = value_.find_last_of(c); return p == std::string::npos ? -1 : (int)p; }
    String substring(size_t pos) const { return pos < value_.size() ? String(value_.substr(pos)) : String(); }
    bool endsWith(const char *s) const {
        std::string suffix = s ? s : "";
        return value_.size() >= suffix.size() &&
               value_.compare(value_.size()-suffix.size(), suffix.size(), suffix) == 0;
    }
    void toLowerCase() { std::transform(value_.begin(), value_.end(), value_.begin(),
         [](unsigned char c){ return (char)std::tolower(c); }); }
    bool operator==(const char *s) const { return value_ == (s ? s : ""); }
    bool operator==(const String &s) const { return value_ == s.value_; }
    bool operator!=(const String &s) const { return value_ != s.value_; }
};
