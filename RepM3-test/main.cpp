#include "repM3.h"
#include "repM3provider.h"
#include <iostream>

int main(int argc, char** argv)
{
  //test static
  lgmc::RepM3Test tst;
  std::cout << tst.hello() << std::endl;

  //test header only
  lgmc::RepM3ProviderTest tst2;
  std::cout << tst2.hello() << std::endl;

  return 0;
}
