#include "database.h"
#include "parser.h"
using namespace std;

Database::Database()
{
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
                   store[tokens[1]] = tokens[2];
                }
                else if(tokens[0]=="DEL" && tokens.size()>=2)
                {
                    store.erase(tokens[1]);
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
    if(aof_file.is_open())
    {
        aof_file<<"DEL"<<key<<endl;
        aof_file.flush();
    }
   
    store.erase(key);
}