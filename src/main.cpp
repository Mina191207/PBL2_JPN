#include "vocabulary.h"
#include <iostream>
#include <string>
using namespace std;
int main() {
    vocabulary ob1("1", "こんにちは", "konnichiwa", "Hello", "greeting", "こんにちは、元気ですか？");
    cout << ob1.getWord() << endl;
    return 0;
};