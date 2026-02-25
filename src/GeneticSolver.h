#pragma once

#include "ISolver.h"
#include "Instance.h" // Убедитесь, что подключили заголовочный файл с TestInstance
#include "Mecler/Parameters.h"

class GeneticSolver : public ISolver {
public:
    GeneticSolver(double maxTimeSeconds) {name =  "Mecler"; timeLimit = maxTimeSeconds;}




    long long ComputeSolution(const TestInstance& instance) override;
    Parameters* createParametersFromInstance(const TestInstance& instance);

};
