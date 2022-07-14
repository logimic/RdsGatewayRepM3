#include "MyTraceHex.h"
#include <exception>
#include <algorithm>
#include <sstream>
#include <vector>

namespace lgmc {
  /// \brief Parse binary data encoded hexa
  /// \param [out] to buffer for result binary data
  /// \param [in] from hexadecimal string
  /// \details
  /// Gets hexadecimal string in form e.g: "00 a5 b1" (space separation) or "00.a5.b1" (dot separation) and parses to binary data
  std::vector<uint8_t> parseBinary(const std::string& from)
  {
    std::vector<uint8_t> retVect;
    if (!from.empty()) {

      std::string buf = from;
      std::replace(buf.begin(), buf.end(), '.', ' ');
      std::istringstream istr(buf);

      int val;
      while (istr >> std::hex >> val) {
        retVect.push_back((uint8_t)val);
      }
      if (!istr.eof()) {
        throw std::logic_error("Unexpected format");
      }
      return retVect;
    }
  }

  /// \brief Encode binary data to hexa string
  /// \param [in] len length of dat to be encoded
  /// \return encoded string
  /// \details
  /// Encode binary data to hexadecimal string in form e.g "00.a5.b1" (dot separation)
  std::string encodeBinary(const std::vector<uint8_t> & from)
  {
    std::string to;
    if (from.size() > 0) {
      std::ostringstream ostr;
      ostr << shape::TracerMemHex(from.data(), from.size(), '.');
      to = ostr.str();
      if (to[to.size() - 1] == '.') {
        to.pop_back();
      }
    }
    return to;
  }

}
