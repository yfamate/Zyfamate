#include <cstdlib>
#include <cstdio>
#include "zyfamate_core.h"

int main() {
    Zyfamate::init_tables();
    Zyfamate::usi_loop();
    return 0;
}
