#include "utils.h"
#include <cstring>

using namespace sxako;
using namespace std;

char *to_plain(string s) {
  char *result=new char[s.size()+1];
  strcpy(result, s.c_str());
  return result;
}

int main(int argc, char *argv[]) {
  if (argc==1) {
    int argc_augmented=argc+3;
    char **argv_augmented=new char *[argc_augmented+1];
    argv_augmented[0]=argv[0];
    argv_augmented[1]=to_plain("-H");
    argv_augmented[2]=to_plain("-V");
    argv_augmented[3]=to_plain("material_and_position:.04");
    copy(&argv[1], &argv[argc+1], &argv_augmented[4]);
    return play_game(argc_augmented, argv_augmented,
                     cin_input, cout_output, cout_output);
  }
  else
  return play_game(argc, argv,
                   cin_input, cout_output, cout_output);
}
