#if !defined(ARGS_H)
#define ARGS_H

#include <string>
#include <vector>

typedef enum
{
  CL_IR,
  CL_ASM,
  CL_OBJ,
  CL_EXE,
} CompilationLevel;

typedef struct
{
  std::vector<std::string> filenames;
  std::string output_name;
  std::string libs;
  bool nostdlib;
  CompilationLevel compilation_level;

  bool pic;
} Settings;

class ArgParser
{
private:
  char **argv;
  int argc;
  int index;

public:
  ArgParser(int argc, char **argv);

  std::string next();
};

#endif // ARGS_H
