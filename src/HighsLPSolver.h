#pragma once

#include "ISolver.h"
#include "Instance.h"

// Подключение HiGHS (путь может отличаться в зависимости от установки)
#include <Highs.h>


class HighsLPSolver : public ISolver {
public:
    HighsLPSolver(){name = "LP_HiGHS";}


    long long ComputeSolution(const TestInstance& instance) override;
};
