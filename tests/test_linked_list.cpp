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

    for (int i = 0; i < numbers.size(); i++) {
        std::cout << numbers.at(i) << " ";
    }

    numbers.pushBack(60);

    return 0;
}