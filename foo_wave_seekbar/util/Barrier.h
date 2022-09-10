#pragma once

#include <atomic>

namespace util {
struct barrier
{
    unsigned const count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;

    barrier(unsigned count_)
      : count(count_)
      , spaces(count_)
      , generation(0)
    {}

    void wait()
    {
        unsigned const my_generation = generation;
        if (!--spaces) {
            spaces = count;
            ++generation;
        } else {
            while (generation == my_generation)
                ;
        }
    }
};
}