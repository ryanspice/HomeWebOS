#pragma once
#include <string>
inline std::string prepared_fixture_png(){
  const std::string hex="89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c6310d1b069000001c500f90550acc20000000049454e44ae426082";
  auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
  std::string out;
  for(size_t i=0;i<hex.size();i+=2)out.push_back(static_cast<char>(digit(hex[i])*16+digit(hex[i+1])));
  return out;
}
