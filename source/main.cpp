// Zyfamate: standalone USI tsume-shogi (mate) engine.
//   usi / isready / setoption name USI_Hash value <MB> / position (startpos|sfen ...) [moves ...] / go mate ... / stop / quit
#include <cstdlib>
#include <cstdio>
#include "zyfamate_core.h"

int main() {
    Zyfamate::init_tables();
    Zyfamate::usi_loop();
    return 0;
}
