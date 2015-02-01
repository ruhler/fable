#ifndef LOCATION_H_
#define LOCATION_H_

#include <iostream>
#include <string>

struct Location {
  int line;
  int column;
  std::string source;
};

bool operator==(const Location& a, const Location& b);
std::ostream& operator<<(std::ostream& os, const Location& a);

#endif//LOCATION_H_

