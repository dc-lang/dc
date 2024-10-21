#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

std::string readFile(const std::string &filename)
{
  std::ifstream file(filename);
  if (!file)
  {
    std::cerr << "Error opening file: " << filename << "\n";
    exit(1);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return buffer.str();
}
