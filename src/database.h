#include<string>
#include<unordered_map>
#include <fstream>
#include <iostream>
#include <list>
#include <utility>
#include <chrono>

using namespace std;

#pragma once

struct Cacheitem
{
    string value;
    list<string>::iterator lru_it;
    long long expire_time;
};

class Database
{
    private:
    
    list<string>lru_dll;
    unordered_map<string,Cacheitem>store;
    ofstream aof_file;
    int capacity;

    public:
    Database();
    ~Database();
    void set(string key,string value);
    string get(string key);
    void del(string key);
    void expire(string key,int seconds);
};


