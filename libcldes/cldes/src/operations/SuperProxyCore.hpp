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

 File: cldes/src/operations/SuperProxyCore.hpp
 Description: SuperProxy methods definitions
 =========================================================================
*/
/*!
 * \file cldes/src/operations/SuperProxyCore.hpp
 *
 * \author Adriano Mourao \@madc0ww
 * \date 2018-06-16
 *
 * Virtual Proxy for the monolithic supervisor synthesis.
 */

namespace cldes {
template<uint8_t NEvents, typename StorageIndex>
op::SuperProxy<NEvents, StorageIndex>::SuperProxy(
  DESystemBase const& aPlant,
  DESystemBase const& aSpec,
  EventsTableHost const& aNonContr)
  : cldes::DESystemBase<
      NEvents,
      StorageIndex,
      op::SuperProxy<NEvents, StorageIndex>>{ aPlant.getStatesNumber() *
                                                aSpec.getStatesNumber(),
                                              aSpec.getInitialState() *
                                                  aPlant.getStatesNumber() +
                                                aPlant.getInitialState() }
  , sys0_{ aPlant }
  , sys1_{ aSpec }
{
    n_states_sys0_ = aPlant.getStatesNumber();

    auto const in_both = aPlant.getEvents() & aSpec.getEvents();

    only_in_plant_ = aPlant.getEvents() ^ in_both;
    only_in_spec_ = aSpec.getEvents() ^ in_both;

    // Initialize events_ from base class
    this->events_ = aPlant.getEvents() | aSpec.getEvents();

    for (auto q0 : aPlant.getMarkedStates()) {
        for (auto q1 : aSpec.getMarkedStates()) {
            this->marked_states_.emplace(q1 * n_states_sys0_ + q0);
        }
    }

    findRemovedStates_(aPlant, aSpec, aNonContr);
}

// template<uint8_t NEvents, typename StorageIndex>
// op::SuperProxy<NEvents, StorageIndex>::SuperProxy(
//   DESVector<NEvents, StorageIndex> const& aPlants,
//   DESVector<NEvents, StorageIndex> const& aSpecs,
//   EventsTableHost const& aNonContr)
// {
//     // TODO: Implement this function
// }

template<uint8_t NEvents, typename StorageIndex>
void
op::SuperProxy<NEvents, StorageIndex>::findRemovedStates_(
  DESystemBase const& aP,
  DESystemBase const& aE,
  EventsTableHost const& aNonContr) noexcept
{
    SyncSysProxy<NEvents, StorageIndex> virtualsys{ aP, aE };
    // non_contr in a bitarray structure
    EventsSet<NEvents> non_contr_bit;
    EventsSet<NEvents> p_non_contr_bit;
    // Evaluate which non contr event is in system and convert it to a
    // bitarray
    for (cldes::ScalarType event : aNonContr) {
        if (aP.getEvents().test(event)) {
            p_non_contr_bit.set(event);
            if (virtualsys.events_.test(event)) {
                non_contr_bit.set(event);
            }
        }
    }
    // Supervisor states
    StatesTableHost<StorageIndex> rmtable;
    // f is a stack of states accessed in a dfs
    StatesStack<StorageIndex> f;
    // Initialize f and ftable with the initial state
    f.push(virtualsys.init_state_);
    // Allocate inverted graph, since we are search for inverse transitions
    virtualsys.allocateInvertedGraph();
    while (!f.empty()) {
        auto const q = f.top();
        f.pop();
        if (!rmtable.contains(q) && !virtual_states_.contains(q)) {
            auto const qx = q % virtualsys.n_states_sys0_;
            bool badstate = false;
            for (ScalarType event = 0; event < NEvents; ++event) {
                if (p_non_contr_bit.test(event) && aP.trans(qx, event) != -1 &&
                    virtualsys.trans(q, event) == -1) {
                    badstate = true;
                    break;
                }
            }
            if (badstate) {
                // TODO: Fix template implicit instantiation
                removeBadStates<NEvents, StorageIndex>(
                  virtualsys, virtual_states_, q, non_contr_bit, rmtable);
            } else {
                virtual_states_.insert(q);
                cldes::ScalarType event = 0;
                EventsSet<NEvents> event_it;
                event_it.set();
                while (event_it.any()) {
                    auto const fsqe = virtualsys.trans(q, event);
                    if (fsqe != -1 && event_it.test(0)) {
                        if (!rmtable.contains(fsqe)) {
                            if (!virtual_states_.contains(fsqe)) {
                                f.push(fsqe);
                            }
                        }
                    }
                    ++event;
                    event_it >>= 1;
                }
            }
        }
    }
    rmtable.clear();
    this->states_number_ = virtual_states_.size();
    virtualsys.clearInvertedGraph();
    //trim();
    return;
}

template<uint8_t NEvents, typename StorageIndex>
op::SuperProxy<NEvents, StorageIndex>::operator DESystem() noexcept
{
    std::sort(virtual_states_.begin(), virtual_states_.end());
    synchronizeStage2(*this);

    // Allocate memory for the real sys
    auto sys_ptr = std::make_shared<DESystem>(DESystem{});

    sys_ptr->states_number_ = std::move(this->states_number_);
    sys_ptr->init_state_ = std::move(this->init_state_);
    sys_ptr->marked_states_ = std::move(this->marked_states_);
    sys_ptr->events_ = std::move(this->events_);

    // Resize adj matrices
    sys_ptr->graph_.resize(this->states_number_, this->states_number_);
    sys_ptr->bit_graph_.resize(this->states_number_, this->states_number_);

    // Move triplets to graph storage
    sys_ptr->graph_.setFromTriplets(triplet_.begin(), triplet_.end());

    triplet_.clear();

    sys_ptr->graph_.makeCompressed();
    sys_ptr->bit_graph_.makeCompressed();

    return *sys_ptr;
}

template<uint8_t NEvents, typename StorageIndex>
bool
op::SuperProxy<NEvents, StorageIndex>::containstrans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const noexcept
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return false;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;

    auto const in_x = sys0_.containstrans(qx, aEvent);
    auto const in_y = sys1_.containstrans(qy, aEvent);

    auto contains = false;

    if ((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
        (in_y && only_in_spec_.test(aEvent))) {
        contains = true;
    }

    return contains;
}

template<uint8_t NEvents, typename StorageIndex>
typename op::SuperProxy<NEvents, StorageIndex>::StorageIndexSigned
op::SuperProxy<NEvents, StorageIndex>::trans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const noexcept
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return -1;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;

    auto const in_x = sys0_.containstrans(qx, aEvent);
    auto const in_y = sys1_.containstrans(qy, aEvent);

    if (!((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
          (in_y && only_in_spec_.test(aEvent)))) {
        return -1;
    }

    if (in_x && in_y) {
        auto const q0 = sys0_.trans(qx, aEvent);
        auto const q1 = sys1_.trans(qy, aEvent);

        return q1 * n_states_sys0_ + q0;
    } else if (in_x) {
        auto const q0 = sys0_.trans(qx, aEvent);
        return qy * n_states_sys0_ + q0;
    } else { // in_y
        auto const q1 = sys1_.trans(qy, aEvent);
        return q1 * n_states_sys0_ + qx;
    }
}

template<uint8_t NEvents, typename StorageIndex>
bool
op::SuperProxy<NEvents, StorageIndex>::containsinvtrans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return false;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;

    auto const in_x = sys0_.containsinvtrans(qx, aEvent);
    auto const in_y = sys1_.containsinvtrans(qy, aEvent);

    auto contains = false;

    if ((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
        (in_y && only_in_spec_.test(aEvent))) {
        contains = true;
    }

    return contains;
}

template<uint8_t NEvents, typename StorageIndex>
StatesArray<StorageIndex>
op::SuperProxy<NEvents, StorageIndex>::invtrans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const
{
    StatesArray<StorageIndex> inv_transitions;

    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return inv_transitions;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;

    auto const in_x = sys0_.containsinvtrans(qx, aEvent);
    auto const in_y = sys1_.containsinvtrans(qy, aEvent);

    if (!((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
          (in_y && only_in_spec_.test(aEvent)))) {
        return inv_transitions;
    }

    if (in_x && in_y) {
        auto const inv_trans_0 = sys0_.invtrans(qx, aEvent);
        auto const inv_trans_1 = sys1_.invtrans(qy, aEvent);

        inv_transitions.reserve(inv_trans_0.size() + inv_trans_1.size());

        for (auto q0 : inv_trans_0) {
            for (auto q1 : inv_trans_1) {
                auto const q_from = q1 * n_states_sys0_ + q0;
                inv_transitions.push_back(q_from);
            }
        }
    } else if (in_x) {
        auto const inv_trans_0 = sys0_.invtrans(qx, aEvent);

        inv_transitions.reserve(inv_trans_0.size());

        for (auto q : inv_trans_0) {
            auto const q_from = qy * n_states_sys0_ + q;
            inv_transitions.push_back(q_from);
        }
    } else { // in_y
        auto const inv_trans_1 = sys1_.invtrans(qy, aEvent);

        inv_transitions.reserve(inv_trans_1.size());

        for (auto q : inv_trans_1) {
            auto const q_from = q * n_states_sys0_ + qx;
            inv_transitions.push_back(q_from);
        }
    }

    return inv_transitions;
}

template<uint8_t NEvents, typename StorageIndex>
void
op::SuperProxy<NEvents, StorageIndex>::allocateInvertedGraph_impl() const
  noexcept
{
    sys0_.allocateInvertedGraph();
    sys1_.allocateInvertedGraph();
}

template<uint8_t NEvents, typename StorageIndex>
void
op::SuperProxy<NEvents, StorageIndex>::clearInvertedGraph_impl() const noexcept
{
    sys0_.clearInvertedGraph();
    sys1_.clearInvertedGraph();
}

template<uint8_t NEvents, typename StorageIndex>
void
op::SuperProxy<NEvents, StorageIndex>::trim() noexcept
{
    StatesTableHost<StorageIndex> trimmed_virtual_states;
    for (auto mstate : this->marked_states_) {
        StatesStack<StorageIndex> f;
        f.push(mstate);
        while (!f.empty()) {
            auto const q = f.top();
            f.pop();
            trimmed_virtual_states.insert(q);
            cldes::ScalarType event = 0;
            EventsSet<NEvents> event_it;
            event_it.set();
            while (event_it.any()) {
                auto const fsqelist = this->invtrans(q, event);
                if (event_it.test(0) && !fsqelist.empty()) {
                    for (auto fsqe : fsqelist) {
                        if (virtual_states_.contains(fsqe)) {
                            if (!trimmed_virtual_states.contains(fsqe)) {
                                f.push(fsqe);
                            }
                        }
                    }
                }
                ++event;
                event_it >>= 1;
            }
        }
    }
    virtual_states_ = trimmed_virtual_states;
    this->states_number_ = virtual_states_.size();
    return;
}
}
