#pragma once

#include <string>

namespace astralia {

class UserService {
  public:
    UserService();

    const std::string &name() const { return name_; }
    std::string uptime() const;

  private:
    std::string name_;
};

} // namespace astralia
