#include "parser.h"
#include<sstream>

// Implementation of the Parser class methods will go here
vector<string> Parser::split(string input)
{
    vector<string>words;
    stringstream ss(input);
    string word;
    while(ss >> word)
    {
        words.push_back(word);
    }

    return words;

}