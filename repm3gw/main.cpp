#include "VersionInfo.h"
#include <Shaper.h>
#include <StaticComponentMap.h>
#include <Trace.h>
#include <iostream>

TRC_INIT_MNAME("chsgw");
#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
//Shape buffer channel
#define TRC_CHANNEL 0


int main(int argc, char** argv)
{
#if defined(_WIN32) && defined(_DEBUG)
  const std::string catch_mem_leak_test = "mem_leak_test";
  char* catch_mem_leak_test_ptr = shape_new(char[catch_mem_leak_test.size() + 1]);
  strcpy(catch_mem_leak_test_ptr, catch_mem_leak_test.data());
#endif

  if (argc == 2 && argv[1] == std::string("version")) {
    std::cout << APP_VERSION << " " << APP_BUILD_TIMESTAMP << std::endl;
    return 0;
  }

  std::ostringstream header;
  header <<
      "============================================================================" << std::endl <<
      PAR(APP_VERSION) << PAR(APP_BUILD_TIMESTAMP) << std::endl << std::endl <<
      "Copyright 2018 Logimic, s.r.o." << std::endl <<
      "============================================================================" << std::endl;

  std::cout << header.str();
  TRC_INFORMATION(header.str());

  std::cout << "startup ... " << std::endl;
  shapeInit(argc, argv);
  shapeRun();
  return 0;
}
