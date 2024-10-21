#if !defined(ARGS_H)
#define ARGS_H

#include <string>

typedef enum
{
  CL_IR,
  CL_ASM,
  CL_OBJ,
  CL_EXE,
} CompilationLevel;

typedef struct
{
  std::string filename;
  std::string libs;
  CompilationLevel compilation_level;

  std::string getFileNameNoExtenstion();

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
