#pragma once

#include "ISolver.h"
#include "Instance.h"
#include "Highs.h"
#include <string>

class HighsMCFSolver final : public ISolver {
public:
    HighsMCFSolver(double seconds){name = "MCF_ILP"; timeLimit = seconds;}
    double ComputeSolution(const TestInstance& instance) override;


};
