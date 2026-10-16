#include <iostream>
#include <string>
using namespace std;

class favorite{
    string id;
    string user_id;
    string resource_id;
    public:
        favorite(string, string, string);
        ~favorite();
};