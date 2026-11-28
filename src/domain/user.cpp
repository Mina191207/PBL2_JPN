#include "domain/user.h"
#include "domain/Validation.h"
#include <sodium.h>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
constexpr unsigned long long opsLimit = 2;
constexpr std::size_t memLimit = 64 * 1024 * 1024;

void requireSodium() {
    static const bool ready = sodium_init() >= 0;
    if (!ready) throw CredentialError("password library initialization failed");
}

std::string encodePassword(const std::string& password) {
    requireSodium();
    char encoded[crypto_pwhash_argon2id_STRBYTES];
    if (crypto_pwhash_argon2id_str(encoded, password.data(), password.size(), opsLimit, memLimit) != 0)
        throw CredentialError("password hashing failed");
    return encoded;
}

void validateNewPassword(const std::string& password) {
    if (password.empty() || password.size() > maxPasswordBytes ||
        password.front() == ' ' || password.front() == '\t' ||
        password.back() == ' ' || password.back() == '\t')
        throw std::invalid_argument("invalid password");
    validateTxtField(password, "password", false);
}

bool takePrefix(std::string_view& text, std::string_view prefix) {
    if (text.substr(0, prefix.size()) != prefix) return false;
    text.remove_prefix(prefix.size());
    return true;
}

bool takeNumber(std::string_view& text, unsigned min, unsigned max) {
    if (text.empty() || text.front() == '0') return false;
    unsigned number = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    if (result.ec != std::errc{} || number < min || number > max)
        return false;
    text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
    return true;
}

template<std::size_t bytes> bool validBase64(std::string_view text) {
    unsigned char decoded[bytes];
    std::size_t size = 0;
    return text.size() == (bytes * 8 + 5) / 6 &&
        sodium_base642bin(decoded, bytes, text.data(), text.size(), nullptr, &size, nullptr,
                         sodium_base64_VARIANT_ORIGINAL_NO_PADDING) == 0 && size == bytes;
}

bool validHash(std::string_view text) {
    // Bound the work before handing a value from disk to the Argon2 verifier.
    if (text.size() >= crypto_pwhash_argon2id_STRBYTES ||
        !takePrefix(text, "$argon2id$v=19$m=") || !takeNumber(text, 19456, memLimit / 1024) ||
        !takePrefix(text, ",t=2,p=1$"))
        return false;
    requireSodium();
    const auto separator = text.find('$');
    return separator != std::string_view::npos &&
        validBase64<crypto_pwhash_argon2id_SALTBYTES>(text.substr(0, separator)) &&
        validBase64<32>(text.substr(separator + 1));
}
}

User::User(UserId id, std::string username, const std::string& password, UserRole role, UserStatus status)
    : User(std::move(id), std::move(username), std::string{}, role, status, StoredCredential{}) {
    resetPassword(password);
}

User::User(UserId id, std::string username, std::string hash, UserRole role, UserStatus status, StoredCredential)
    : id_(std::move(id)),
      username_(std::move(username)),
      passwordHash_(std::move(hash)),
      role_(role),
      status_(status) {
    validateTxtField(username_, "username");
    if (username_.size() < 3 || username_.size() > 32 ||
        !((username_[0] >= 'A' && username_[0] <= 'Z') ||
          (username_[0] >= 'a' && username_[0] <= 'z'))) {
        throw std::invalid_argument("invalid username");
    }
    for (char c : username_) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '-')) {
            throw std::invalid_argument("invalid username");
        }
    }
    if ((role_ != UserRole::ADMIN && role_ != UserRole::LEARNER) ||
        (status_ != UserStatus::ACTIVE && status_ != UserStatus::LOCKED))
        throw std::invalid_argument("invalid user role or status");
}

const UserId& User::getId() const noexcept {
    return id_;
}
const std::string& User::getUsername() const noexcept {
    return username_;
}
UserRole User::getRole() const noexcept {
    return role_;
}
UserStatus User::getStatus() const noexcept {
    return status_;
}

bool User::checkPassword(const std::string& password) const {
    requireSodium();
    return crypto_pwhash_argon2id_str_verify(passwordHash_.c_str(), password.data(), password.size()) == 0;
}

bool User::rehashPassword(const std::string& verifiedPassword) {
    const int needed = crypto_pwhash_argon2id_str_needs_rehash(passwordHash_.c_str(), opsLimit, memLimit);
    if (needed < 0) throw CredentialError("password hash check failed");
    if (needed == 0) return false;
    passwordHash_ = encodePassword(verifiedPassword);
    return true;
}

User User::restore(UserId id, std::string username, const std::string& hash, UserRole role, UserStatus status) {
    if (!validHash(hash)) throw std::invalid_argument("invalid or unsupported password hash");
    return User(std::move(id), std::move(username), hash, role, status, StoredCredential{});
}

void User::setStatus(UserStatus status) {
    if (status != UserStatus::ACTIVE && status != UserStatus::LOCKED)
        throw std::invalid_argument("invalid status");
    status_ = status;
}

void User::resetPassword(const std::string& password) {
    validateNewPassword(password);
    passwordHash_ = encodePassword(password);
}
