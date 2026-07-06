#include "database.h"
#include "parser.h"
using namespace std;

Database::Database()
{
     capacity=4;
    ifstream in_file("database.aof");
    if(in_file.is_open())
    {
        string line;
        Parser parser;

        while(getline(in_file,line))
        {
            vector<string>tokens=parser.split(line);
            if(tokens.size()>0)
            {
                if(tokens[0]=="SET" && tokens.size()>=3)
                {
                   set(tokens[1], tokens[2]);
                }
                else if(tokens[0]=="DEL" && tokens.size()>=2)
                {
                    del(tokens[1]);
                }
            }
        }

        in_file.close();
        printf("Database reloaded from disk\n");
    }
    aof_file.open("database.aof",ios::app);
    if(!aof_file.is_open())
    {
        printf("Crtical ERROR:Could not open file for writing \n");
    }
}

Database::~Database()
{
    if(aof_file.is_open())
    aof_file.close();
}
// Implementation of the Database class methods will go here
void Database::set(string key,string value)
{
    if(aof_file.is_open())
    {
        aof_file<<"SET "<<key<<" "<<value<<endl;
        aof_file.flush();
    }
   if(store.find(key)!=store.end())
   {
    store[key].value=value;
    lru_dll.splice(lru_dll.begin(), lru_dll, store[key].lru_it);
    store[key].expire_time=0;
    return;
   }
   if(store.size()>=capacity)
   {
    string oldest=lru_dll.back();
    lru_dll.pop_back();
    store.erase(oldest);
   }
   lru_dll.push_front(key);

    store[key]={value,lru_dll.begin(),0};
}

string Database::get(string key)
{
   
    if(store.find(key)!=store.end())
    {
        long long current_time=chrono::system_clock::now().time_since_epoch()/chrono :: seconds(1);
        if(store[key].expire_time != 0 && current_time >= store[key].expire_time)
        {
            del(key);
            return "";
        }

        lru_dll.splice(lru_dll.begin(),lru_dll,store[key].lru_it);
        return store[key].value;
    }
    else
    {
        return "";
    }
}
void Database::del(string key)
{
    if(aof_file.is_open())
    {
        aof_file<<"DEL"<<key<<endl;
        aof_file.flush();
    }
   if(store.find(key)!=store.end())
   {
       lru_dll.erase(store[key].lru_it);
       store.erase(key);
   }
}
void Database::expire(string key,int seconds)
{
    if(store.find(key)!=store.end())
    {
        long long current_time=chrono::system_clock::now().time_since_epoch()/chrono::seconds(1);
        long long death_time=current_time+seconds;
        store[key].expire_time=death_time;
        if(aof_file.is_open())
        {
            aof_file << "EXPIRE " << key << " " << seconds << endl;
            aof_file.flush();
        }
    }
}
