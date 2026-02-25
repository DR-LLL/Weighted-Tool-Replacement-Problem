#pragma once
#include "DAG.h"
#include "Instance.h"
#include "string"
class IBuildGraph {
public:
    virtual ~IBuildGraph() = default;

    // Собирает DAG для данного инстанса. Если K<=0 — реализация сама выберет безопасное K.
    virtual DAG build_from_instance(const TestInstance& instance, int K = 0) const = 0;

    // Имя билдера (для логов/CSV).
    std::string name;
};
