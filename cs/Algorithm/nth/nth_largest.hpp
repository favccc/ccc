#pragma once

#include <cstddef>

class NthLargest {
   public:
    using index = std::size_t;
    NthLargest(int *data, index num, index nth);
    int nth();
    static void register_test();

   private:
    index group(index begin, index last, index sep);
    int nth(index begin, index last, index nth);
    int *data_ = nullptr;
    std::size_t size_ = 0;
    index nth_ = 0;
};