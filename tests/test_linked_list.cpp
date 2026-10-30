#include "dsa/LinkedList.h"
#include <iostream>

bool lessInt(const int& a, const int& b) {
    return a < b;
}

int main() {
    LinkedList<int> numbers;

    numbers.pushBack(50);
    numbers.pushBack(20);
    numbers.pushBack(40);
    numbers.pushBack(10);
    numbers.pushBack(30);

    numbers.mergeSort(lessInt);

    int expected[] = {10, 20, 30, 40, 50};

for (int i = 0; i < 5; i++) {
    if (numbers.at(i) != expected[i]) {
        return 1;
    }
}

numbers.pushBack(60);

if (numbers.size() != 6 || numbers.at(5) != 60) {
    return 1;
}
    for (int i = 0; i < numbers.size(); i++) {
        std::cout << numbers.at(i) << " ";
    }
    return 0;
}