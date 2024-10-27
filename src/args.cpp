#include <args.hpp>
#include <algorithm>

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