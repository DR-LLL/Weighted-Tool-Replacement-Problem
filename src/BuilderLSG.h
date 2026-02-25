#pragma once

#include "IBuildGraph.h"

// Графовая модель с переносом инструментов и финальной вершиной.
// Реализует описанную тобой формулировку.
class LSGBuilder final : public IBuildGraph {
public:
    LSGBuilder(){name ="LSG";}


    // If K > 0, a penalty is applied for mandatory edges.
    // If K <= 0, it is selected automatically.
    DAG build_from_instance(const TestInstance& inst, int K = 0) const override;
};
