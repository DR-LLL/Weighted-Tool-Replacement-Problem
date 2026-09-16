#include "Instance.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include "iostream"
using namespace std;

static inline bool is_comment(const string& s) {
    for(size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        if(!isspace(ch)) return s[i] == '#';
    }
    return true;
}

static vector<string> split_tokens(const string& s) {
    vector<string> r;
    string tok;
    istringstream in(s);
    while(in >> tok) r.push_back(tok);
    return r;
}

TestInstance TestInstance::load_from_file(const string& filename) {
    TestInstance inst;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        throw runtime_error("Cannot open file: " + filename);
    }

    auto next_line = [&](istream& in, string& out) -> bool {
        while (getline(in, out)) {
            if(!is_comment(out)) return true;
        }
        return false;
    };

    // M N C
    if(!next_line(file, line)) throw runtime_error("Unexpected EOF before M N C");
    {
        vector<string> tok = split_tokens(line);
        if(tok.size() != 3) throw runtime_error("Expected: M N C");
        inst.M = stoi(tok[0]);
        inst.N = stoi(tok[1]);
        inst.C = stoi(tok[2]);
    }

    // COST line
    if(!next_line(file, line)) throw runtime_error("Unexpected EOF before COST");
    {
        vector<string> tok = split_tokens(line);
        if(tok.empty() || tok[0] != "COST") throw runtime_error("Expected 'COST'");
        if((int)tok.size() != 1 + inst.M) throw runtime_error("COST line must have M values");

        inst.tool_costs.assign(inst.M, 0);
        for(int j = 0; j < inst.M; ++j) {
            inst.tool_costs[j] = stoll(tok[1 + j]);
        }
    }

    // JOBS line
    if(!next_line(file, line)) throw runtime_error("Unexpected EOF before JOBS");
    {
        vector<string> tok = split_tokens(line);
        if(tok.size() != 2 || tok[0] != "JOBS") throw runtime_error("Expected 'JOBS N'");
        int n2 = stoi(tok[1]);
        if(n2 != inst.N) throw runtime_error("JOBS N mismatch");
    }

    // Job requirements
    inst.job_requirements.clear();
    inst.job_requirements.reserve(inst.N);

    for(int r = 0; r < inst.N; ++r) {
        if(!next_line(file, line)) throw runtime_error("Unexpected EOF reading job line");
        vector<string> tok = split_tokens(line);
        if(tok.empty()) throw runtime_error("Empty job line");
        //std::cout << "line:" << line << std::endl;
        int k = stoi(tok[0]);
        //std::cout << k << " (k)" << std::endl;
        if((int)tok.size() != 1 + k) throw runtime_error("Job line wrong token count");

        set<int> tools;
        //std::cout << "tools for " << r << std::endl;
        for(int i = 0; i < k; ++i) {
            int t = stoi(tok[1 + i]);
            if(t < 1 || t > inst.M) throw runtime_error("Tool id out of range");
            tools.insert(t);
            //std::cout << t << ", ";

        }
        //std::cout << "all" << std::endl;
        inst.job_requirements.push_back(tools);

    }

    file.close();
    return inst;
}