#include "repM3.h"
#include "HexStringCoversion.h"

using namespace lgmc;
using namespace rapidjson;

int main(int argc, char** argv)
{
  GetVersion ver;
  
  // prepare request
  std::vector<uint8_t> request = ver.serialize();
  std::cout << std::endl << "request: " << encodeBinary(request);

  // receive response
  ver.deserialize(parseBinary("B1.03.09.05.01.02.00.14.B2"));

  // read data
  GetVersion::Version v = ver.getVersion();
  std::cout << std::endl << "response: "
    << " major=" << v.major
    << " minor=" << v.minor
    << " relase=" << v.release
    << " variant=" << v.variant
    << std::endl;

  return 0;
}
