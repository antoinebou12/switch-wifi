#include "switch_wifi/core/metrics.hpp"

#include <cassert>
#include <iostream>

using namespace swifi;

int main() {
    RingHistory<int> h(3);
    assert(h.empty());
    h.push(1);
    h.push(2);
    h.push(3);
    h.push(4);
    const auto values = h.values();
    assert(values.size() == 3);
    assert(values[0] == 2 && values[1] == 3 && values[2] == 4);
    assert(h.capacity() == 3);
    h.clear();
    assert(h.empty());

    RingHistory<int> minimumCapacity(0);
    minimumCapacity.push(7);
    minimumCapacity.push(8);
    const auto one = minimumCapacity.values();
    assert(one.size() == 1 && one.front() == 8);

    std::cout << "history tests passed\n";
    return 0;
}
