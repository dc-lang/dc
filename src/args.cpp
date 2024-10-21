#include <args.hpp>
#include <algorithm>

std::string Settings::getFileNameNoExtenstion()
{
  size_t lastSlash = this->filename.find_last_of("/\\");
  size_t lastDot = this->filename.find_last_of('.');

  if (lastSlash == std::string::npos)
  {
    return (lastDot == std::string::npos) ? this->filename : this->filename.substr(0, lastDot);
  }

  std::string filename = this->filename.substr(lastSlash + 1);

  return (lastDot == std::string::npos || lastDot < lastSlash) ? filename : filename.substr(0, lastDot - lastSlash - 1);
}

ArgParser::ArgParser(int argc, char **argv)
{
  this->argv = argv;
  this->argc = argc;
  this->index = 1;
};

std::string ArgParser::next()
{
  this->index++;
  if (this->index > argc)
  {
    return "";
  }

  return argv[this->index - 1];
}