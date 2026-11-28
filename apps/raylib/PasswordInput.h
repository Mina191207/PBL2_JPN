#pragma once

#include "pbl2/InputLimits.h"
#include "Utf8.h"
#include <string>

class PasswordInput {
    std::string value_;
    bool tooLong_ = false;
    static void wipe(char* buffer, std::size_t count) noexcept {
        volatile char* bytes = buffer;
        for (std::size_t i = 0; i < count; ++i) bytes[i] = 0;
    }

  public:
    PasswordInput() { value_.reserve(maxPasswordBytes); }
    ~PasswordInput() { clear(); }
    PasswordInput(const PasswordInput&) = delete;
    PasswordInput& operator=(const PasswordInput&) = delete;
    const std::string& value() const noexcept { return value_; }
    bool tooLong() const noexcept { return tooLong_; }
    std::string masked() const {
        return std::string(Utf8::countValid(value_), '*');
    }
    void clear() noexcept {
        wipe(value_.data(), value_.size());
        value_.clear();
        tooLong_ = false;
    }
    void backspace() {
        if (value_.empty()) return;
        const auto start = Utf8::lastOffset(value_);
        wipe(value_.data() + start, value_.size() - start);
        value_.resize(start);
        tooLong_ = false;
    }
    void append(int codepoint) {
        if (codepoint != '\t' && (codepoint < 32 || codepoint == 127)) return;
        auto encoded = Utf8::encode(codepoint);
        if (value_.size() + encoded.size() <= maxPasswordBytes) value_ += encoded;
        else tooLong_ = true;
        wipe(encoded.data(), encoded.size());
    }
};
