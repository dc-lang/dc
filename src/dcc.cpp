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

#if 1
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
extern i32 printf str vararg;
extern i32 scanf str vararg;
extern ptr malloc i64;
extern void free ptr;
extern void exit i32;
extern i64 strtol str str* i32;


"Memory Allocations"

context alloc i64 __size -> ptr;
declare ptr __ptr;

malloc(__size) -> __ptr;

return __ptr;
context;

context delete ptr __ptr -> void;

free(__ptr);

return;
context;


"Collapses"

context collapse_handler str desc -> void;

printf("Program collapsed: %s\n", desc);

exit(128);
return;
context;

context collapse str desc -> void;

collapse_handler(desc);

return;
context;


"Parse functions"

context parse_int str buff -> i32;
declare i32 result;
declare str end;
declare str* end_p;
declare i32 si;
declare i64 sl;
declare i8 c;

assign end_p -> end;

strtol(buff, end_p, 10) -> sl;

if end == buff;
collapse("[parse_int] parse failed");
fi;

deref end -> c;

if 0 != c;
collapse("[parse_int] parse failed");
fi;

return sl;

context;

)";
  }
  int stdlib_newlines = 0;
  for (int i = 0; i < input.size(); i++)
  {
    if (input.at(i) == '\n')
    {
      stdlib_newlines++;
    }
  }

  input += "\n" + readFile(settings.filename);
  Lexer lexer(input, stdlib_newlines);

  compile(lexer, settings);
}
