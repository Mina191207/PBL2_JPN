#include <iostream>
#include <string>
using namespace std;

class user{
    string id;
    string username;
    string password;
    string role;
    string active;
    public:
    user(string, string, string, string, string);
    ~user();
    void getinline(user&);
    void getoutline(const user&);
};