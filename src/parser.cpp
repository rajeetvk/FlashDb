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

vector<string> Parser::parse(string input)
{
    vector<string> tokens;
    
    if (input.empty() || input[0] != '*') 
    {
        return split(input);
    }
    
    int pos = 0;
    
    pos = input.find("\r\n", pos);
    if (pos == string::npos) return tokens;
    pos += 2;
    
    while (pos < input.length()) 
    {
        if (input[pos] == '$') 
        {
            int rn_pos = input.find("\r\n", pos);
            if (rn_pos == string::npos) break;
            
            string len_str = input.substr(pos + 1, rn_pos - pos - 1);
            int len = 0;
            try {
                len = stoi(len_str);
            } catch (...) {
                break;
            }
            
            pos = rn_pos + 2; 
            
            string word = input.substr(pos, len);
            tokens.push_back(word);
            
            pos = pos + len + 2; 
        } 
        else 
        {
            break;
        }
    }
    
    return tokens;
}