#include <iostream>
#include <string>
using namespace std;

class vocabulary{
    string id;
    string word;
    string reading;
    string meaning;
    string type;
    string example;
    public:
        vocabulary(string, string, string, string, string, string);
        ~vocabulary();
};