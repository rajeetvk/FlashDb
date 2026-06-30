#include<string>
#include<unordered_map>
using namespace std;

#pragma once

class Database
{
    private:
    unordered_map<string,string>store;

    public:
    void set(string key,string value);
    string get(string key);
    void del(string key);
};

// The Database class will handle the in-memory key-value store (std::unordered_map)
// It will have methods like get(), set(), and del()
