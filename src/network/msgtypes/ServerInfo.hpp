#ifndef DIGGLER_NET_MSGTYPES_SERVERINFO_HPP
#define DIGGLER_NET_MSGTYPES_SERVERINFO_HPP

#include "MsgType.hpp"

#include <goodform/variant.hpp>

namespace Diggler {
namespace Net {
namespace MsgTypes {

enum class ServerInfoSubtype : uint8 {
  Request = 0,
  Response = 1
};

struct ServerInfoRequest : public MsgType {
  std::vector<std::string> infos;
  goodform::variant params;

  void writeToMsg(OutMessage&) const override;
  void readFromMsg(InMessage&) override;
};

struct ServerInfoResponse : public MsgType {
  goodform::variant infos;

  void writeToMsg(OutMessage&) const override;
  void readFromMsg(InMessage&) override;
};

}
}
}

#endif /* DIGGLER_NET_MSGTYPES_SERVERINFO_HPP */
