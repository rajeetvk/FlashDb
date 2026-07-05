#include<string>
#include<unordered_map>
#include <fstream>
#include <iostream>
#include <list>
#include <utility>

using namespace std;

#pragma once

class Database
{
    private:
    
    list<string>lru_dll;
    unordered_map<string,pair<string,list<string>::iterator>>store;
    ofstream aof_file;
    int capacity;

    public:
    Database();
    ~Database();
    void set(string key,string value);
    string get(string key);
    void del(string key);
};


