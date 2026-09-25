// Deterministic in-memory microSD simulator for ThemeFileService tests only.
#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <algorithm>
extern std::map<std::string,std::string> fakeThemeFiles;
class File {
    std::shared_ptr<const std::string> bytes_;
    size_t pos_ = 0;
public:
    File() {}
    explicit File(const std::string &body) : bytes_(new std::string(body)) {}
    explicit operator bool() const { return bytes_.get() != nullptr; }
    bool isDirectory() const { return false; }
    size_t size() const { return bytes_ ? bytes_->size() : 0; }
    size_t read(uint8_t *buf, size_t n) {
        if (!bytes_) return 0;
        size_t available = bytes_->size() - pos_;
        n = std::min(n,available);
        if (n) { memcpy(buf,bytes_->data()+pos_,n); pos_+=n; }
        return n;
    }
    int read() {
        if (!bytes_ || pos_ >= bytes_->size()) return -1;
        return (uint8_t)(*bytes_)[pos_++];
    }
    bool seek(size_t next) { if (!bytes_ || next > bytes_->size()) return false; pos_=next; return true; }
    int available() const { return bytes_ ? (int)(bytes_->size()-pos_) : 0; }
    void close() { bytes_.reset(); }
};
namespace fs {
class FS {
public:
    File open(const String &path, const char * = FILE_READ) {
        auto it = fakeThemeFiles.find(path.c_str());
        return it == fakeThemeFiles.end() ? File() : File(it->second);
    }
};
}
