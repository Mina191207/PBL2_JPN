#include <iostream>
#include <string>
using namespace std;

class kanji{
    string id;
    string character;
    string stroke_count;
    string onyomi;
    string kunyomi;
    string example;
    public:
        kanji(string, string, string, string, string, string);
        ~kanji();
};