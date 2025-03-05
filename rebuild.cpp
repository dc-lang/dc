#include "_rebuild/rebuild.h"

std::string cflags = "-c -Iinclude -std=c++20";
std::string iflags = "-Iinclude";

int main(int argc, char **argv) {
  system("mkdir -p build");
  rebuild_targets.push_back(
      Target::create("build/dcc",
                     {"build/args.o", "build/dcc.o", "build/fs.o",
                      "build/lexer.o", "build/compiler.o"},
                     "g++ -o #OUT #DEPENDS -lLLVM-19"));

  rebuild_targets.push_back(CTarget::create(
      "build/args.o", {"src/args.cpp"}, "g++ -o #OUT #DEPENDS " + cflags,
      REBUILD_STANDARD_CXX_COMPILER, iflags));
  rebuild_targets.push_back(CTarget::create(
      "build/dcc.o", {"src/dcc.cpp"}, "g++ -o #OUT #DEPENDS " + cflags,
      REBUILD_STANDARD_CXX_COMPILER, iflags));
  rebuild_targets.push_back(CTarget::create(
      "build/fs.o", {"src/fs.cpp"}, "g++ -o #OUT #DEPENDS " + cflags,
      REBUILD_STANDARD_CXX_COMPILER, iflags));
  rebuild_targets.push_back(CTarget::create(
      "build/lexer.o", {"src/lexer.cpp"}, "g++ -o #OUT #DEPENDS " + cflags,
      REBUILD_STANDARD_CXX_COMPILER, iflags));
  rebuild_targets.push_back(CTarget::create(
      "build/compiler.o", {"src/compiler.cpp"},
      "g++ -o #OUT #DEPENDS " + cflags, REBUILD_STANDARD_CXX_COMPILER, iflags));
  return 0;
}
