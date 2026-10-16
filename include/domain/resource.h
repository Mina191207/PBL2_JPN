#include <iostream>
#include <string>
using namespace std;

class resource{
    string id;
    string type;
    string title;
    string level_id;
    public:
        resource(string, string, string, string, string, string);
        ~resource();
};