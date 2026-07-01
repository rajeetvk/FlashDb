#include "database.h"

// Implementation of the Database class methods will go here
void Database::set(string key,string value)
{

   

    store[key]=value;
}

string Database::get(string key)
{
   
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
   
    store.erase(key);
}