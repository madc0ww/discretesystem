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

 File: tests/basics.cpp
 Description: Exemplify the basic usage of the library.
 =========================================================================
*/

#include "cldes/DESystem.hpp"
#include <chrono>
#include <iostream>
#include <set>
#include <string>

#include "testlib.hpp"

using namespace std::chrono;

int
main()
{
    std::cout << "Creating DES" << std::endl;
    int const n_states = 4;

    cldes::DESystem<3>::StatesSet marked_states;
    marked_states.insert(0);
    marked_states.insert(2);

    int const init_state = 0;

    cldes::DESystem<3> sys{ n_states, init_state, marked_states };

    // Declare transitions: represented by prime numbers
    cldes::ScalarType const a = 0;
    cldes::ScalarType const b = 1;
    cldes::ScalarType const g = 2;

    sys(0, 0) = a;
    sys(0, 2) = g;
    sys(1, 0) = a;
    sys(1, 1) = b;
    sys(2, 1) = a;
    sys(2, 1) = g;
    sys(2, 2) = b;
    sys(2, 3) = a;

    auto graph = sys.getGraph();

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    auto accessible_states = sys.accessiblePart();
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(accessible_states, "< Accessible part", "0 1 2 3 >");
    std::cout << "Accessible States time: " << duration << " microseconds"
              << std::endl;

    t1 = high_resolution_clock::now();
    auto coaccessible_states = sys.coaccessiblePart();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(coaccessible_states, "< Coaccessible part", "0 1 2 >");
    std::cout << "Coaccessible States time: " << duration << " microseconds"
              << std::endl;

    t1 = high_resolution_clock::now();
    auto trimstates = sys.trimStates();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(trimstates, "< trim states", "0 1 2 >");
    std::cout << "trim time: " << duration << " microseconds" << std::endl;

    std::cout << "Creating new system" << std::endl;

    cldes::DESystem<3> new_sys{ n_states, init_state, marked_states };

    // This graph has no transition from the 3rd state to th 4th one.
    new_sys(0, 0) = a;
    new_sys(0, 2) = g;
    new_sys(1, 1) = b;
    new_sys(2, 1) = a;
    new_sys(2, 1) = g;
    new_sys(2, 2) = b;
    new_sys(3, 1) = a;
    new_sys(3, 2) = a;

    auto new_graph = new_sys.getGraph();
    //  PrintGraph(new_graph, "New Sys");

    t1 = high_resolution_clock::now();
    auto new_accessible_states = new_sys.accessiblePart();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(new_accessible_states, "< Accessible part", "0 1 2 >");
    std::cout << "Accessible States time: " << duration << " microseconds"
              << std::endl;

    t1 = high_resolution_clock::now();
    auto new_coaccessible_states = new_sys.coaccessiblePart();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(new_coaccessible_states, "< Coaccessible part", "0 2 3 >");
    std::cout << "Coaccessible States time: " << duration << " microseconds"
              << std::endl;

    t1 = high_resolution_clock::now();
    auto new_trimstates = new_sys.trimStates();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    ProcessResult(new_trimstates, "< trim states", "0 2 >");
    std::cout << "trim States time: " << duration << " microseconds"
              << std::endl;

    t1 = high_resolution_clock::now();
    new_sys.trim();
    t2 = high_resolution_clock::now();
    duration = duration_cast<microseconds>(t2 - t1).count();

    std::cout << "trim time: " << duration << " microseconds" << std::endl;

    return 0;
}
