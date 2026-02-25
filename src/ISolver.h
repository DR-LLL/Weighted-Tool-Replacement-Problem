#pragma once
#include <string>
#include "Instance.h"  // для TestInstance

struct ISolver {
    virtual ~ISolver() = default;


    std::string name;

    virtual long long ComputeSolution(const TestInstance& instance) = 0;
    double timeLimit; //seconds
};


