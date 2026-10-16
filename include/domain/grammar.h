#include <iostream>
#include <string>
using namespace std;

class grammar{
    string id;
    string pattern;
    string meaning;
    string structure;
    string use;
    string example;
    string note;
    public:
        grammar(string, string, string, string, string, string, string = "");
        ~grammar();
};