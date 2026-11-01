#pragma once
#include "DataStore.h"
#include <string>

class FileStorage {
  public:
    bool loadAll(const std::string& folder, DataStore& out, std::string& error) const;
    bool saveAll(const std::string& folder, const DataStore& data, std::string& error) const;
    bool loadPackage(const std::string& folder, DataStore& out, std::string& error) const;
    bool savePackage(const std::string& folder, const DataStore& data, std::string& error) const;
};
