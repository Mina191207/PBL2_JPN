#include <iostream>
#include <string>
using namespace std;

class level{
    string id;
    string caterogy;
    string name;
    int order;
    string desciption;
    public:
        level(string, string, string, string, string);
        ~level();
};