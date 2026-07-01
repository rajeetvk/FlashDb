#include "database.h"

// Implementation of the Database class methods will go here
void Database::set(string key,string value)
{

    lock_guard<mutex>lock(mtx);

    store[key]=value;
}

string Database::get(string key)
{
    lock_guard<mutex>lock(mtx);
    if(store.find(key)!=store.end())
    {
        return store[key];
    }
    else
    {
        return "";
    }
}
void Database::del(string key)
{
    lock_guard<mutex>lock(mtx);
    store.erase(key);
}