/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Config.h"

using namespace std;
using namespace ramulator;

Config::Config(const std::string& fname) {
  parse(fname);
}

void Config::parse(const string& fname)
{
    ifstream file(fname);
    assert(file.good() && "Bad config file");
    string line;
    while (getline(file, line)) {
        char delim[] = " \t=";
        vector<string> tokens;

        while (true) {
            size_t start = line.find_first_not_of(delim);
            if (start == string::npos) 
                break;

            size_t end = line.find_first_of(delim, start);
            if (end == string::npos) {
                tokens.push_back(line.substr(start));
                break;
            }

            tokens.push_back(line.substr(start, end - start));
            line = line.substr(end);
        }

        // empty line
        if (!tokens.size())
            continue;

        // comment line
        if (tokens[0][0] == '#')
            continue;

        // parameter line
        assert(tokens.size() == 2 && "Only allow two tokens in one line");

        options[tokens[0]] = tokens[1];

        if (tokens[0] == "channels") {
          channels = atoi(tokens[1].c_str());
        } else if (tokens[0] == "ranks") {
          ranks = atoi(tokens[1].c_str());
        } else if (tokens[0] == "subarrays") {
          subarrays = atoi(tokens[1].c_str());
        } else if (tokens[0] == "cpu_tick") {
          cpu_tick = atoi(tokens[1].c_str());
        } else if (tokens[0] == "mem_tick") {
          mem_tick = atoi(tokens[1].c_str());
        } else if (tokens[0] == "expected_limit_insts") {
          expected_limit_insts = atoi(tokens[1].c_str());
        } else if (tokens[0] == "warmup_insts") {
          warmup_insts = atoi(tokens[1].c_str());
        }
    }
    file.close();
}


