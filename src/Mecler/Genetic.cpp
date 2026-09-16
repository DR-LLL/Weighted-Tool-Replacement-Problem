/**
 * @file Genetic.cpp
 *
 * Implements Genetic class methods
 *
 * @author Jordana Mecler
 *
 * Contact: jmecler@inf.puc-rio.br
 *
 */
#include <chrono>
#include "Genetic.h"

// Main code of the HGA
void Genetic::evolve(int maxIterWithoutImprov) {
    auto t0 = std::chrono::high_resolution_clock::now();
    //std::cout << "ok"<< std::endl;
    // Individuals used for crossover
    Individual *parent1;
    Individual* parent2;

    nbIterWithoutImprov = 1;
    int nbIterWithoutImprovDiv = 1;
    nbIter = 1;

    string temp;
    int place;
    clock_t debut = clock();

    // Child reference
    offspring = new Individual(parameters);
    // Individual used for local search
    trainer = new Individual(parameters);
    trainer->localSearch = new LocalSearch(parameters, trainer);
    long long limitObj = maxIterWithoutImprov / 20;
    while (nbIterWithoutImprov < maxIterWithoutImprov ) {

        //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0 << std::endl;
        // CROSSOVER
        parent1 = population->getIndividualBinT(); // Pick individual by binary tournament
        parent2 = population->getIndividualBinT(); // Pick individual by binary tournament

        crossoverOX(parent1, parent2); // OX crossover

        // Calculates second objective
        offspring->solutionCost.zeroBlocks = offspring->calcZeroBlocks();


        // LOCAL SEARCH
        trainer->recopyIndividual(trainer, offspring);
        trainer->localSearch->runSearchTotal();
        offspring->recopyIndividual(offspring, trainer);

        // Tries to add child to population
        bool useSecondObj = false;
        if (nbIterWithoutImprovDiv > limitObj) {
            useSecondObj = true;
        }
        place = population->addIndividual(offspring, useSecondObj);

        if (place == -2) {
            return;
        }

        if (place == 0) { // A new best solution has been found
            nbIterWithoutImprov = 1;
            nbIterWithoutImprovDiv = 1;
        }
        else
            nbIterWithoutImprov++;

        nbIterWithoutImprovDiv++;
        nbIter++;

        // DIVERSIFICATION
        // Max iterations without improvement resulting in diversification reached
        if (nbIterWithoutImprovDiv == parameters->maxDiversify) {
            population->diversify();
            if (parameters->terminate) {
                return;
            }
            nbIterWithoutImprovDiv = 1;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        //break;
        if (std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0 > ticks * 1000) {
            break;
        }
    }
    parameters->nbIter = (unsigned int) nbIter;
}

void Genetic::crossoverOX(Individual *parent1, Individual *parent2) {

    // Beginning and end of the crossover zone
    unsigned int begin = rand() % parameters->numJobs;
    unsigned int end = rand() % parameters->numJobs;
    while (end == begin && parameters->numJobs > 1)
        end = rand() % parameters->numJobs;

    if (begin > end) {
        unsigned int temp = begin;
        begin = end;
        end = temp;
    }

    // Copy part of parent1 to child
    offspring->chromosome = parent1->chromosome;
    for (unsigned int i = 0; i < parameters->positionsOffspring.size(); i++) {
        parameters->positionsOffspring[i] = false;
    }
    for (unsigned int i = begin; i <= end; i++) {
        parameters->positionsOffspring[parent1->chromosome[i]] = true;
    }

    // Copy unused values of parent2 to child sequentially
    unsigned int pos = end + 1, i = end + 1;
    while (pos < parameters->positionsOffspring.size()) {
        if (!parameters->positionsOffspring[parent2->chromosome[i]]) {
            offspring->chromosome[pos] = parent2->chromosome[i];
            pos++;
        }
        i++;
        if (i == parameters->positionsOffspring.size()) {
            i = 0;
        }
    }
    if (i == parameters->positionsOffspring.size()) {
        i = 0;
    }
    pos = 0;
    while (pos < begin) {
        if (!parameters->positionsOffspring[parent2->chromosome[i]]) {
            offspring->chromosome[pos] = parent2->chromosome[i];
            pos++;
        }
        i++;
        if (i == parameters->positionsOffspring.size()) {
            i = 0;
        }
    }
    offspring->solutionCost.evaluation = offspring->calcCost(-1);
}

Genetic::Genetic(Parameters *parameters, Population *population, double ticks, bool traces) :
        parameters(parameters), population(population), ticks(ticks), traces(traces) {
}

Genetic::~Genetic() {
    delete offspring;
    delete trainer->localSearch;
    delete trainer;
}