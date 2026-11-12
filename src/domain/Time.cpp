#include "domain/Time.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::string utcNow() {
    const std::time_t now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    if (gmtime_s(&value, &now) != 0)
#else
    if (!gmtime_r(&now, &value))
#endif
        throw std::runtime_error("cannot obtain UTC time");
    std::ostringstream out;
    out << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

bool validUtcTimestamp(const std::string& text) {
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z')
        return false;
    for (std::size_t i = 0; i < 19; ++i)
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && (text[i] < '0' || text[i] > '9'))
            return false;
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    if (year < 1 || month < 1 || month > 12)
        return false;
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        days[1] = 29;
    return day >= 1 && day <= days[month - 1] && std::stoi(text.substr(11, 2)) < 24 &&
           std::stoi(text.substr(14, 2)) < 60 && std::stoi(text.substr(17, 2)) < 60;
}
