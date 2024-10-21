#include <stdio.h>
#include <args.hpp>
#include <lexer.hpp>
#include <vector>
#include <compiler.hpp>

int main(int argc, char **argv)
{
  Settings settings;
  ArgParser argparser = ArgParser(argc, argv);

  settings.compilation_level = CL_EXE;
  settings.filename = "";

  while (true)
  {
    std::string arg = argparser.next();
    if (arg == "")
      break;

    if (arg.at(0) != '-')
    {
      settings.filename = arg;
    }
    else
    {
      if (arg == "--help")
      {
        printf("Usage: dcc <file> [options]\n");
        printf("Options:\n");
        printf("  --help                   Display this information\n");
        printf("  --ir (-i)                Generate only IR code\n");
        printf("  --asm (-S)               Generate only assembly\n");
        printf("  --obj (-c)               Generate only object file\n");
        return 0;
      }
      else if (arg == "--ir" || arg == "-i")
      {
        settings.compilation_level = CL_IR;
      }
      else if (arg == "--asm" || arg == "-S")
      {
        settings.compilation_level = CL_ASM;
      }
      else if (arg == "--obj" || arg == "-c")
      {
        settings.compilation_level = CL_OBJ;
      }
      else
      {
        printf("\x1b[1mdcc:\x1b[0m \x1b[1;31merror:\x1b[0m unrecognized command-line option ’%s’\n", arg.c_str());
      }
    }
  }

  if (settings.filename.empty())
  {
    // printf("\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m no input files\n");
    // printf("compilation terminated.\n");
    // return 1;
  }

  std::string input = R"(
extern i32 printf ptr vararg;
extern i32 scanf ptr vararg;

context main i32 argc ptr argv -> i8;

declare i8 ch;
assign ch = 'A';

declare i8* str;

call printf "%c argc: %d\n" ch argc;
return 0;

context;
)";
  Lexer lexer(input);
  compile(lexer, settings);
}