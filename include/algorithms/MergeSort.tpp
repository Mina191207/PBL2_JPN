#include <cstddef>

template <class T, class Compare> void mergeSort(std::vector<T>& values, Compare less) {
    if (values.size() < 2) {
        return;
    }
    std::vector<T> buffer(values);
    auto sort = [&](auto&& self, std::size_t begin, std::size_t end) -> void {
        if (end - begin < 2) {
            return;
        }
        const std::size_t mid = begin + (end - begin) / 2;
        self(self, begin, mid);
        self(self, mid, end);

        std::size_t left = begin;
        std::size_t right = mid;
        for (std::size_t out = begin; out < end; ++out) {
            if (right == end || (left < mid && !less(values[right], values[left]))) {
                buffer[out] = values[left++];
            } else {
                buffer[out] = values[right++];
            }
        }
        for (std::size_t i = begin; i < end; ++i) {
            values[i] = buffer[i];
        }
    };
    sort(sort, 0, values.size());
}
