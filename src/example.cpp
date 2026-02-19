#include "jamanak.hpp"

int main () {

    jamanak::Jamanak durs = jamanak::Jamanak("Counting Durations");

    durs.start("Till 10000");
    for (size_t i = 0; i < 1000; i++){}
    
    durs.end();

    durs.start("Till 10000000");
    for (size_t i = 0; i < 10000000; i++){}
    durs.end();

    durs.start("Till 10000000000");
    for (size_t i = 0; i < 10000000000; i++){}
    durs.end();

    std::printf("%s",durs.to_string().c_str());

    std::puts("");

    for (const auto& j : durs.get_jams()) {
        std::printf("%s: %.6f ms\n", j->context.c_str(), j->duration_ms);
    }

    return 0;
}