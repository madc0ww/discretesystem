/*
 =========================================================================
 This file is part of clDES

 clDES: an OpenCL library for Discrete Event Systems computing.

 clDES is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 clDES is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with clDES.  If not, see <http://www.gnu.org/licenses/>.

 Copyright (c) 2018 - Adriano Mourao <adrianomourao@protonmail.com>
                      madc0ww @ [https://github.com/madc0ww]

 LacSED - Laboratorio de Analise e Controle de Sistemas a Eventos Discretos
 Universidade Federal de Minas Gerais

 File: test/kernels.hpp
 Description: Test cldes::op::synchronize function, the parallel
 composition implementation.
 =========================================================================
*/

#include "cldes/DESystem.hpp"
#include "cldes/operations/Operations.hpp"
#include "clustertool.hpp"
#include "testlib.hpp"
#include <chrono>
#include <cstdlib>
#include <vector>

using namespace std::chrono;

int
main()
{
    using StorageIndex = unsigned;

    std::set<StorageIndex> marked_states;
    cldes::DESystem<16> plant{ 1, 0, marked_states };
    cldes::DESystem<16> spec{ 1, 0, marked_states };
    cldes::DESystem<16>::EventsTable non_contr;

    high_resolution_clock::time_point t1;
    high_resolution_clock::time_point t2;

    {
        std::vector<cldes::DESystem<16>> plants;
        std::vector<cldes::DESystem<16>> specs;

        std::cout << "Generating ClusterTool(2)" << std::endl;
        ClusterTool(2, plants, specs, non_contr);
        std::cout << std::endl;

        std::cout << "Synchronizing plants" << std::endl;
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        auto last_result = plants[0ul];
        plant = last_result;
        for (auto i = 1ul; i < plants.size(); ++i) {
            plant = cldes::op::synchronize(last_result, plants[i]);
            last_result = plant;
        }
        high_resolution_clock::time_point t2 = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(t2 - t1).count();

        std::cout << "Plants sync time spent: " << duration << " microseconds"
                  << std::endl;

        std::cout << "Synchronizing specs" << std::endl;
        t1 = high_resolution_clock::now();
        last_result = specs[0ul];
        spec = last_result;
        for (auto i = 1ul; i < specs.size(); ++i) {
            spec = cldes::op::synchronize(last_result, specs[i]);
            last_result = spec;
        }
        t2 = high_resolution_clock::now();
        duration = duration_cast<microseconds>(t2 - t1).count();

        std::cout << "Specs sync time spent: " << duration << " microseconds"
                  << std::endl;
    }

    std::cout << std::endl
              << "Number of states of plant: " << plant.size() << std::endl;
    std::cout << "Number of transitions of the plant "
              << plant.getGraph().nonZeros() << std::endl;
    std::cout << "Number of states of the spec: " << spec.size() << std::endl;
    std::cout << "Number of transitions of the spec "
              << spec.getGraph().nonZeros() << std::endl
              << std::endl;

    std::cout << "{plant, spec}.trim()" << std::endl;
    t1 = high_resolution_clock::now();
    plant.trim();
    spec.trim();
    t2 = high_resolution_clock::now();

    std::cout << std::endl
              << "Number of states of plant: " << plant.size() << std::endl;
    std::cout << "Number of transitions of the plant "
              << plant.getGraph().nonZeros() << std::endl;
    std::cout << "Number of states of the spec: " << spec.size() << std::endl;
    std::cout << "Number of transitions of the spec "
              << spec.getGraph().nonZeros() << std::endl
              << std::endl;

    auto duration = duration_cast<microseconds>(t2 - t1).count();
    std::cout << "trim time spent: " << duration << " microseconds"
              << std::endl;

    std::cout << std::endl;
    std::cout << "Computing the supervisor" << std::endl;
    t1 = high_resolution_clock::now();
    auto supervisor = cldes::op::supC(plant, spec, non_contr);
    t2 = high_resolution_clock::now();

    duration = duration_cast<microseconds>(t2 - t1).count();
    std::cout << "Supervisor synth time spent: " << duration << " microseconds"
              << std::endl;

    std::cout << "Number of states of the supervisor: " << supervisor.size()
              << std::endl;
    std::cout << "Number of transitions of the supervisor "
              << supervisor.getGraph().nonZeros() << std::endl;
}
