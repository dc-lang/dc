#include <stdio.h>
#include <args.hpp>
#include <lexer.hpp>
#include <fs.hpp>
#include <vector>
#include <compiler.hpp>

int main(int argc, char **argv)
{
  Settings settings;
  ArgParser argparser = ArgParser(argc, argv);

  settings.compilation_level = CL_EXE;
  settings.filename = "";
  settings.libs = "";
  settings.pic = true;
  settings.nostdlib = false;

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
        printf("  --nostdlib                Disable standard library\n");
        printf("  -l <lib>                 Link libraries\n");
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
      else if (arg == "--nostdlib")
      {
        settings.nostdlib = true;
      }
      else if (arg == "-l")
      {
        settings.libs += argparser.next() + " ";
      }
      else
      {
        printf("\x1b[1mdcc:\x1b[0m \x1b[1;31merror:\x1b[0m unrecognized command-line option ’%s’\n", arg.c_str());
        return 1;
      }
    }
  }

#if 0
  settings.filename = "/home/aceinet/dc/build/a.dc";
#endif
  if (settings.filename.empty())
  {
    printf("\x1b[1mdcc:\x1b[0m \x1b[1;31mfatal error:\x1b[0m no input files\n");
    printf("compilation terminated.\n");
    return 1;
  }

  std::string input = "";
  if (!settings.nostdlib)
  {
    input = R"(
extern i32 printf i8* vararg;
extern i32 scanf i8* vararg;
)";
  }
  input += "\n" + readFile(settings.filename);
  Lexer lexer(input);

  compile(lexer, settings);
}
