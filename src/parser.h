#pragma once

#include "database.h"
#include <string>
#include <vector>

using namespace std;

class Parser{
    public:

    vector<string> parse(string input);
    vector<string> split(string input);

};

// The Parser class will handle reading the raw string from the client
// and breaking it down into an actionable command (like splitting "SET key value")
