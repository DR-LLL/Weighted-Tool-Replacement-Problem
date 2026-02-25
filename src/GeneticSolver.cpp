#include "GeneticSolver.h"

#include "Mecler/Population.h"
#include "Mecler/Parameters.h"
#include "Mecler/Genetic.h"

#define min2(a, b) ((a < b) ? a : b)



long long GeneticSolver::ComputeSolution(const TestInstance& instance) {
    //std::cout << "oks"<< std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    double bestCost;
    double averageCost, averageTime;
    //std::cout << "op0" << std::endl;
    unsigned int runs = 1;
    Parameters *parameters;
    Population *population;
    clock_t nb_ticks_allowed;
    double cost, totalAvgCost = 0, totalBestCost = 0;
    double totalTime = 0;
    // Конвертируем TestInstance в параметры, необходимые для генетического алгоритма
    parameters = createParametersFromInstance(instance);


    parameters->setAuxiliaryParams();

    //std::cout << "ok0"<< std::endl;

    // Start population
    //std::cout << "op1" << std::endl;
    population = new Population(parameters, timeLimit);
    auto t1 = std::chrono::high_resolution_clock::now();
    //std::cout << "op2" << std::endl;
    //std::cout << "ok2"<< std::endl;

    // Run genetic algorithm
    const clock_t startTime = clock();

    Genetic solver(parameters, population, timeLimit - std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000000.0, false);
    solver.evolve(min2(parameters->numJobs * 20, 1000));
    //std::cout << parameters->numJobs << "," << parameters->numTools << "," << parameters->maxCapacity << "," << cost << ","  << totalTime << endl;
    cost = population->getBestIndividual()->solutionCost.costs;
    totalTime = (float(clock() - startTime) / CLOCKS_PER_SEC);





    // Освобождаем память
    delete population;
    delete parameters;

    return cost;
}
Parameters* GeneticSolver::createParametersFromInstance(const TestInstance& instance) {
    // Используем конструктор Parameters для инициализации параметров
    Parameters* parameters = new Parameters(
            0,  // seed, пусть будет 0, если не используется
            "", // instancesPaths, можно оставить пустым
            "", // instancesNames, можно оставить пустым
            "", // solutionPath, можно оставить пустым
            20, // populationSize
            40, // maxPopulationSize
            10, // numberElite
            3,  // numberCloseIndividuals
            1000 // maxDiversify
    );

    // Заполняем параметры из TestInstance
    parameters->numJobs = instance.N;
    parameters->numTools = instance.M;
    parameters->maxCapacity = instance.C;



    // Преобразуем job_requirements в формат, подходящий для алгоритма
    parameters->jobsToolsMatrix.clear();
    parameters->toolsCosts = instance.tool_costs;
    for (const auto& reqs : instance.job_requirements) {

        std::vector<unsigned int> job_tools(instance.M, 0);
        for (int tool : reqs) {
            //std::cout << tool << ",: ";
            job_tools[tool-1] = 1;  // Устанавливаем 1 для требуемых инструментов
        }
        parameters->jobsToolsMatrix.push_back(job_tools);
        //std::cout << std::endl;
    }
    //std::cout << parameters->jobsToolsMatrix[0].size();

    return parameters;
}