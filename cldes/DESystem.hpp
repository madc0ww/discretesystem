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

 LacSED - Laboratório de Sistemas a Eventos Discretos
 Universidade Federal de Minas Gerais

 File: cldes/DESystem.hpp
 description: DESystem template class declaration and definition . DESystem is a
 graph, which is modeled as a Sparce Adjacency Matrix.
 =========================================================================
*/
/*!
 * \file cldes/DESystem.hpp
 *
 * \author Adriano Mourao \@madc0ww
 * \date 2018-06-16
 *
 * DESystem template class declaration and definition . DESystem is a
 * graph, which is modeled as a Sparce Adjacency Matrix.
 */

#ifndef DESYSTEM_HPP
#define DESYSTEM_HPP

#include "cldes/DESystemBase.hpp"
#include "cldes/src/des/DESystemCore.hpp"

/*
 * Forward declarations and some useful alias definitions
 */
namespace cldes {

/*! \class DESystem
 *  \brief A discrete-events system on host memory
 *  \details DESystem implements a discrete-event system as a graph stored on
 *  a sparse adjacency matrix.
 *
 *  Single systems operations are available as member functions:
 *
 *  |    Operation         |       Method           |
 *  |:--------------------:|:----------------------:|
 *  | Accessible part      | AccessiblePart()       |
 *  | Coaccessible part    | CoaccessiblePart()     |
 *  | Get trim states      | TrimStates()           |
 *  | Trim                 | Trim()                 |
 *  | Transition           | Trans()                |
 *  | InvTransition        | InvTrans()             |
 *
 *  Where S is the number of states, E the number of events and T the number
 *  of transitions (considering a events OR expression a single transition)
 *
 * \tparam NEvents Number of events: max 255
 * \tparam StorageIndex Unsigned type used for indexing the ajacency matrix
 */
template<uint8_t NEvents, typename StorageIndex>
class DESystem : public DESystemBase<NEvents, StorageIndex>
{
public:
    /*! \brief Base alias
     * \details Alias to base class with implicit template params
     */
    using DESystemBase = DESystemBase<NEvents, StorageIndex>;

    /*! \brief StorageIndex signed type
     * \details Eigen uses signed indexes
     */
    using StorageIndexSigned = typename std::make_signed<StorageIndex>::type;

    /*! \brief EventsSet
     *  \details Set containing 8bit intergets which represent events.
     */
    using EventsSet = EventsSet<NEvents>;

    /*! \brief Adjacency matrix of bitarrays implementing a directed graph
     * \details The graph represents the DES automata:
     * Non zero element: contains at least one transition
     * Each non 0 bit of each element: event that lead to the next
     * stage
     * * row index: from state
     * * col index: to state
     */
    using GraphHostData = Eigen::SparseMatrix<EventsSet, Eigen::RowMajor>;

    /*! \brief Set of states type
     *  \details Set containg unsigned interget types which represent states.
     */
    using StatesSet = typename DESystemBase::StatesSet;

    /*! \brief Table of transitions on a STL container
     *  \details Vector containing bitsets which represent the events that a
     *  state represented by the vector index contains.
     */
    using StatesEventsTable = typename DESystemBase::StatesEventsTable;

    /*! \brief Bit adjacency matrix implementing a directed graph
     * \details Sparse matrix implementing a graph with an adjacency matrix.
     * The graph represents a simplified DES automata:
     * * Non zero element: contains at least one transition
     * * row index: from state
     * * col index: to state
     */
    using BitGraphHostData = Eigen::SparseMatrix<bool, Eigen::RowMajor>;

    /*! \brief Adjacency matrix of bit implementing searching nodes
     * \details Structure used for traversing the graph using a linear algebra
     * approach
     */
    using StatesVector = Eigen::SparseMatrix<bool, Eigen::ColMajor>;

    /*! \brief Set of Events implemented as a Hash Table for searching
     * efficiently.
     *  \details Sparse hash set used to encapsulate events (8bit intergers)
     */
    using EventsTable = spp::sparse_hash_set<uint8_t>;

    /*! \brief Vector of states type
     *  \details Vector of usigned interger type which represent states.
     */
    using StatesTable = typename DESystemBase::StatesTable;

    /*! \brief Vector of inverted transitions
     * \ details f(s, e) = s_out -> this vector stores = (s_out, (s, e))
     */
    using TrVector =
      std::vector<std::pair<StorageIndex, InvArgTrans<StorageIndex>*>>;

    /*! \brief Graph const iterator
     * \details Used to iterate over the bfs result
     */
    using ColIteratorConst = Eigen::InnerIterator<StatesVector const>;

    /*! \brief Graph const iterator
     * \details Used to iterate over the adjacency matrix
     */
    using RowIterator = Eigen::InnerIterator<GraphHostData const>;

    /*! \brief Graph iterator
     * \details Used to iterate over the adjacency matrix
     */
    using RowIteratorGraph = Eigen::InnerIterator<GraphHostData>;

    /*! \brief Triplet type
     * \details 3-tuples (qfrom, qto, event) for filling adjacency matrix
     * efficiently
     */
    using Triplet = Triplet<NEvents>;

    /*! \brief Default default constructor
     *  \details Creates an empty system:
     *  * Number of states = 0
     *  * Initial state = 0
     *  * Marked states = { }
     *  * Bit graph = Identity
     *  * Graph = Empty matrix with 0 rows and 0 columns
     */
    inline DESystem()
    {
        inv_graph_ = nullptr;
        is_cache_outdated_ = false;
        this->marked_states_ = StatesSet{};
        graph_ = GraphHostData{};
        bit_graph_ = BitGraphHostData{};

        // Initialize bit_graph_ with identity for searching
        bit_graph_.setIdentity();
    }

    /*! \brief DESystem constructor
     * \details Create a system with certain parameters.
     *
     * @param aStatesNumber Number of states of the system
     * @param aInitState System's initial state
     * @param aMarkedStates System's marked states
     * @param aDevCacheEnabled Enable or disable device cache for graph data
     */
    inline DESystem(StorageIndex const& aStatesNumber,
                    StorageIndex const& aInitState,
                    StatesSet& aMarkedStates,
                    bool const& aDevCacheEnabled = true)
      : DESystemBase{ aStatesNumber, aInitState }
    {
        dev_cache_enabled_ = aDevCacheEnabled;
        is_cache_outdated_ = true;
        inv_graph_ = nullptr;

        this->marked_states_ = aMarkedStates;

        // Resize graphs and do not preserve elements
        graph_ =
          GraphHostData{ static_cast<StorageIndexSigned>(aStatesNumber),
                         static_cast<StorageIndexSigned>(aStatesNumber) };
        bit_graph_ =
          BitGraphHostData{ static_cast<StorageIndexSigned>(aStatesNumber),
                            static_cast<StorageIndexSigned>(aStatesNumber) };

        // Initialize bit graph with Identity
        bit_graph_.setIdentity();

        // Change graphs storage type to CSR
        graph_.makeCompressed();
        bit_graph_.makeCompressed();

        // Reserve memory to make insertions efficient
        this->states_events_ = StatesEventsTable(aStatesNumber);
        this->inv_states_events_ = StatesEventsTable(aStatesNumber);

        // If device cache is enabled, cache it
        if (dev_cache_enabled_) {
            this->CacheGraph_();
        }
    }

    /*! \brief DESystem destructor
     *  \details Override base destructor for avoiding memory leaks.
     */
    ~DESystem() = default;

    /*! \brief Move constructor
     *  \details Enable move semantics
     */
    DESystem(DESystem&&) = default;

    /*! \brief Copy constructor
     *  \details Enable copy by reference of a const system.
     */
    DESystem(DESystem const&) = default;

    /*! \brief Operator =
     *  \details Use move semantics with operator = when assigning to rvalues.
     */
    DESystem<NEvents, StorageIndex>& operator=(DESystem&&) = default;

    /*! \brief Operator = to const type
     *  \details Enable copy by reference of const systems with operator =.
     */
    DESystem<NEvents, StorageIndex>& operator=(DESystem const&) = default;

    /*! \brief Clone method for polymorphic copy
     *  \return Shared pointer to this object
     */
    inline std::shared_ptr<DESystemBase> Clone() const override
    {
        std::shared_ptr<DESystemBase> this_ptr =
          std::make_shared<DESystem>(*this);
        return this_ptr;
    }

    /*! \brief Check if this system is a virtual proxy
     *
     *  \details Operations between two systems return virtual proxies to
     *  allow lazy evaluation of large systems while it is not necessary
     *  to obtain the whole system. While the system is virtual, the
     *  memory complexity of DESystem is cheaper, and the time complexities
     *  of its embedded operations are more expensive.
     *
     *  \return True: DESystem is always a real object
     */
    inline bool IsVirtual() const override { return false; }

    /*! \brief Graph getter
     *
     *  \return Eigen sparse matrix of bitset representing the sysmte on
     *  compressed mode.
     */
    GraphHostData GetGraph() const { return graph_; };

    /*! \brief Returns events that lead a transition between two states
     *
     * @param aQfrom State where the transition departs.
     * @param aQto State where the transition arrives.
     * \return A bitset where each set bit index is a event that lead to the
     * next state.
     */
    inline EventsSet const operator()(StorageIndex const& aQfrom,
                                      StorageIndex const& aQto) const
    {
        return graph_.coeff(aQfrom, aQto);
    }

    /*! \brief Returns reference events that lead a transition between two
     * states
     *  \details This operator can be used for getting events from a transition
     *  between two systems or to assign it. It was implemented to keep track
     *  when the copy of its system on the device memory is outdated.
     *
     * @param aQfrom State where the transition departs.
     * @param aQto State where the transition arrives.
     * \return A reference to the event's transition.
     */
    inline TransitionProxy<NEvents, StorageIndex> operator()(
      StorageIndex const& aQfrom,
      StorageIndex const& aQto)
    {
        return TransitionProxy<NEvents, StorageIndex>(this, aQfrom, aQto);
    }

    /*! \brief Returns state set containing the accessible part of automa
     * \details Executes a breadth-first search in bit the graph, which represents the
     * DES, starting from its initial state. It returns a set containing all nodes
     * which are accessible from the initial state. This operation is
     * implemented on a linear algebra approach by a multiplication between
     * two sparse matrices.
     *
     * \return A set of the accessed states
     */
    StatesSet AccessiblePart() const
    {
        // Executes a BFS on graph_
        auto paccessible_states = Bfs_();

        auto accessible_states = *paccessible_states;
        delete[] paccessible_states;

        return accessible_states;
    }

    /*! \brief Returns state set containing the coaccessible part of automata
     * \details Inverts the bit graph by transposing it (does not store it
     * on the bit_graph_ member). Makes a breadth-first search starting from
     * the marked states all at once by multiplying it by a sparse matrix with
     * states_number_ rows and marked_states_.size() columns.
     *
     * \return A set containing the accessed states by the bfs (coacessible
     * states)
     */
    StatesSet CoaccessiblePart() const
    {
        auto const invgraph = bit_graph_.transpose();

        StorageIndexSigned const n_marked = this->marked_states_.size();
        StatesVector x{ static_cast<StorageIndexSigned>(this->states_number_),
                        n_marked };
        x.reserve(this->marked_states_.size());

        {
            auto pos = 0ul;
            for (auto state : this->marked_states_) {
                x.coeffRef(state, pos) = true;
                ++pos;
            }
        }

        StatesVector y{ static_cast<StorageIndexSigned>(this->states_number_),
                        n_marked };
        auto n_accessed_states = 0l;
        for (StorageIndex i = 0ul; i < this->states_number_; ++i) {
            y = invgraph * x;

            if (n_accessed_states == y.nonZeros()) {
                break;
            } else {
                n_accessed_states = y.nonZeros();
            }

            x = y;
        }

        y.pruned();

        StatesSet coaccessible_states;
        for (StorageIndexSigned s = 0; s < y.outerSize(); ++s) {
            for (ColIteratorConst e(y, s); e; ++e) {
                coaccessible_states.emplace(e.row());
            }
        }

        return coaccessible_states;
    }

    /*! \brief Get states set which represent the trim states of the current
     * system
     * \details Gets the intersection between the accessible part and the
     * coaccessible part.
     *
     * \return A set containing the trim states intergers.
     */
    StatesSet TrimStates() const
    {
        auto accpartstl = AccessiblePart();
        spp::sparse_hash_set<StorageIndex> accpart;
        for (StorageIndex s : accpartstl) {
            accpart.insert(s);
        }

        auto const invgraph = bit_graph_.transpose();

        auto const n_marked = this->marked_states_.size();

        StatesVector x{ static_cast<StorageIndexSigned>(this->states_number_),
                        static_cast<StorageIndexSigned>(n_marked) };
        x.reserve(this->marked_states_.size());
        std::vector<BitTriplet> xtriplet;

        {
            auto pos = 0l;
            for (StorageIndex state : this->marked_states_) {
                xtriplet.push_back(BitTriplet(state, pos, true));
                ++pos;
            }
        }
        x.setFromTriplets(xtriplet.begin(),
                          xtriplet.end(),
                          [](bool const&, bool const&) { return true; });

        StatesVector y{ static_cast<StorageIndexSigned>(this->states_number_),
                        static_cast<StorageIndexSigned>(n_marked) };
        auto n_accessed_states = 0l;
        for (auto i = 0ul; i < this->states_number_; ++i) {
            y = invgraph * x;

            if (n_accessed_states == y.nonZeros()) {
                break;
            } else {
                n_accessed_states = y.nonZeros();
            }

            x = y;
        }

        y.pruned();

        StatesSet trimstates;
        for (auto s = 0l; s < y.outerSize(); ++s) {
            for (ColIteratorConst e(y, s); e; ++e) {
                if (accpart.find(e.row()) != accpart.end()) {
                    trimstates.emplace(e.row());
                }
            }
        }

        return trimstates;
    }

    /*! \brief Returns DES which is the Trim part of this
     * \details Cut the non-trim states from the current system.
     * TODO: Make it lazy. Maybe setting values by 0 and making the graph_
     * compressed again would be a good solution.
     *
     * \return Reference to this system
     */
    DESystemBase& Trim()
    {
        auto trimstates = this->TrimStates();

        if (trimstates.size() == static_cast<size_t>(graph_.rows())) {
            return *this;
        }

        // States map: old state pos -> new state pos
        std::vector<StorageIndexSigned> statesmap(this->states_number_, -1);

        // Copy graph and resize it
        auto const old_graph = graph_;
        this->states_number_ = trimstates.size();
        graph_.resize(static_cast<StorageIndexSigned>(this->states_number_),
                      static_cast<StorageIndexSigned>(this->states_number_));
        bit_graph_.resize(
          static_cast<StorageIndexSigned>(this->states_number_),
          static_cast<StorageIndexSigned>(this->states_number_));

        if (this->states_events_.size() > 0) {
            this->states_events_.erase(this->states_events_.begin() +
                                         this->states_number_,
                                       this->states_events_.end());
            this->inv_states_events_.erase(this->inv_states_events_.begin() +
                                             this->states_number_,
                                           this->inv_states_events_.end());
        }

        this->events_.reset();

        // Calculate the sparsity pattern
        auto sparcitypattern = this->events_.count() * this->states_number_;

        // Fill statesmap
        {
            auto d = 0ul;
            for (StorageIndex s : trimstates) {
                statesmap[s] = d;
                ++d;
            }
        }

        std::vector<Triplet> triplet;
        std::vector<BitTriplet> bittriplet;

        triplet.reserve(sparcitypattern);
        bittriplet.reserve(sparcitypattern);

        // Build new graph_ slice by slice
        {
            auto row_id = 0ul;
            for (StorageIndex s : trimstates) {
                if (this->states_events_.size() > 0) {
                    this->states_events_[row_id].reset();
                    this->inv_states_events_[row_id].reset();
                }
                for (RowIteratorGraph e(old_graph, s); e; ++e) {
                    if (statesmap[e.col()] != -1) {
                        auto const col_id = statesmap[e.col()];

                        triplet.push_back(Triplet(row_id, col_id, e.value()));
                        bittriplet.push_back(BitTriplet(col_id, row_id, true));
                        this->events_ |= e.value();
                        if (this->states_events_.size() > 0) {
                            this->states_events_[row_id] |= e.value();
                            this->inv_states_events_[col_id] |= e.value();
                        }
                    }
                }
                ++row_id;
            }
        }

        // Remove aditional space
        graph_.setFromTriplets(triplet.begin(), triplet.end());
        bit_graph_.setFromTriplets(
          bittriplet.begin(), bittriplet.end(), [](bool const&, bool const&) {
              return true;
          });

        graph_.makeCompressed();
        bit_graph_.makeCompressed();

        // Calculate new marked states
        auto const old_marked = this->marked_states_;
        this->marked_states_.clear();
        for (StorageIndex s : old_marked) {
            if (statesmap[s] != -1) {
                this->marked_states_.emplace(statesmap[s]);
            }
        }

        return *this;
    }

    /*! \brief Insert events
     * \details Set the member events_ with a set containing all events
     * that are present
     * on the current system.
     * \warning Take care using this method. It was designed for
     * testing and debugging.
     *
     * @param aEvents Set containing all the new events of the current system
     * \return void
     */
    void InsertEvents(EventsSet const& aEvents)
    {
        this->events_ = EventsSet(aEvents);
    }

    /*! \brief Check if transition exists
     * \details given a transition f(q, e) = qto, it
     * checks if qto is different of empty.
     *
     * @param aQ State
     * @param aEvent Event
     * \return Returns true if DES transition exists, false otherwise
     */
    inline bool ContainsTrans(StorageIndex const& aQ,
                              ScalarType const& aEvent) const override
    {
        return this->states_events_[aQ].test(aEvent);
    }

    /*! \brief Transition function
     * \details given a transition f(q, e) = qto, it
     * calculates if qto is different of empty.
     *
     * @param aQ State
     * @param aEvent Event
     * \return The state where the transition leads or -1 when it is empty
     */
    inline StorageIndexSigned Trans(StorageIndex const& aQ,
                                    ScalarType const& aEvent) const override
    {
        using RowIterator = Eigen::InnerIterator<
          DESystem<NEvents, StorageIndex>::GraphHostData const>;

        if (!this->states_events_[aQ].test(aEvent)) {
            return -1;
        }

        for (RowIterator qiter(graph_, aQ); qiter; ++qiter) {
            if (qiter.value().test(aEvent)) {
                return qiter.col();
            }
        }

        // It will never reach here, but the warning is bothering me
        return -1;
    }

    /*! \brief Check if the current system contains at least one inverse
     * transition
     * \warning This method requires to run AllocateInvGraph previously. It
     * can be very expensive for large systems.
     *
     * @param aQ State
     * @param aEvent Event
     * \return True if DES inverse transition exists, false otherwise
     */
    inline bool ContainsInvTrans(StorageIndex const& aQ,
                                 ScalarType const& aEvent) const override
    {
        return this->inv_states_events_[aQ].test(aEvent);
    }

    /*! \brief DES Inverse Transition function
     * \warning This method requires to run AllocateInvGraph previously. It
     * can be very expensive for large systems, because it implies in
     * transposing the adjacency matrix.
     *
     * @param aQ State
     * @param aEvent Event
     * \return DES states array containing the inverse transition.
     * It may be empty, if there is none.
     */
    inline StatesArray<StorageIndex> InvTrans(
      StorageIndex const& aQ,
      ScalarType const& aEvent) const override
    {
        StatesArray<StorageIndex> inv_trans;

        if (!this->inv_states_events_[aQ].test(aEvent)) {
            return inv_trans;
        }

        for (RowIterator qiter(*inv_graph_, aQ); qiter; ++qiter) {
            if (qiter.value().test(aEvent)) {
                inv_trans.push_back(qiter.col());
            }
        }

        // It will never reach here, but the warning is bothering me
        return inv_trans;
    }

    /*! \brief Get events of all transitions of a specific state
     * \details Since this is information is stored on a vector on concrete
     * systems, this operation is really cheap, O(1).
     *
     * @param aQ A state on the sys
     * \return Returns EventsSet relative to state q
     */
    inline EventsSet GetStateEvents(StorageIndex const& aQ) const override
    {
        return this->states_events_[aQ];
    }

    /*! \brief Get events of all transitions that lead to a specific state
     * \details Since this is information is stored on a vector on concrete
     * systems, this operation is really cheap, O(1).
     *
     * @param aQ A state on the sys
     * \return Returns EventsSet relative to inverse transitions of the state q
     */
    inline EventsSet GetInvStateEvents(StorageIndex const& aQ) const override
    {
        return this->inv_states_events_[aQ];
    }

    /*! \brief Invert graph
     * \details This is used in the monolithic supervisor synthesis. It needs
     * to be executed before making inverse transitions operations.
     * It is const, since it changes only a mutable member
     * \warning It can be very inneficient for very large graphs
     *
     * \return void
     */
    inline void AllocateInvertedGraph() const override
    {
        inv_graph_ = std::make_shared<GraphHostData>(graph_.transpose());
    }

    /*! \brief Free inverted graph
     * It is const, since it changes only a mutable member
     * \warning It is recommended to clear the it make the inverted graph clear
     * as much as possible when handling large systems.
     *
     * \return void
     */
    inline void ClearInvertedGraph() const override { inv_graph_ = nullptr; }

protected:
    /*! \brief Method for caching the graph
     * \details Copy the graph after transposing it to the device memory.
     *
     * \return void
     */
    void CacheGraph_() { is_cache_outdated_ = false; }

    /*! \brief Method for updating the graph
     * \details Refresh the existent graph data on device memory.
     *
     * \return void
     */
    void UpdateGraphCache_() { is_cache_outdated_ = false; }

    /*! \brief Setup BFS and return accessed states array
     * \details Executes a breadth-first search on the graph starting from N
     * nodes in aInitialNodes. The algorithm is based on SpGEMM.
     *
     * @param aInitialNodes Set of nodes where the searches will start
     * @param aBfsVisit Function to execute on every accessed state: it can be
     * null
     * \return Return a set containing the accessed states, or nullptr if none
     * was accessed or a bfs visit function was defined
     */
    template<class StatesType>
    StatesSet* Bfs_(
      StatesType const& aInitialNodes,
      std::function<void(StorageIndex const&, StorageIndex const&)> const&
        aBfsVisit) const
    {
        /*
         * BFS on a Linear Algebra approach:
         *     Y = G^T * X
         */
        // There is no need of search if a marked state is coaccessible
        StatesVector host_x{
            static_cast<StorageIndexSigned>(this->states_number_),
            static_cast<StorageIndexSigned>(aInitialNodes.size())
        };

        // GPUs does not allow dynamic memory allocation. So, we have
        // to set X on host first.
        std::vector<StorageIndex> states_map;
        for (auto state : aInitialNodes) {
            host_x.coeffRef(state, states_map.size()) = true;
            // Maping each search from each initial node to their correspondent
            // vector on the matrix
            states_map.push_back(state);
        }

        return BfsCalc_(host_x, aBfsVisit, &states_map);
    }

    /*! \brief Overload Bfs_ for the special case of a single initial node
     * \details Executes a breadth-first search on the graph starting from the
     * specified node The algorithm is based on SpGEMM.
     *
     * @param aInitialNode Node where the search will start
     * @param aBfsVisit Function to execute on every accessed state: it can be
     * null
     * \return Return a set containing the accessed states, or nullptr if none
     * was accessed or a bfs visit function was defined
     */
    StatesSet* Bfs_(
      StorageIndex const& aInitialNode,
      std::function<void(StorageIndex const&, StorageIndex const&)> const&
        aBfsVisit) const
    {
        StatesVector host_x{
            static_cast<StorageIndexSigned>(this->states_number_), 1
        };

        /*!
         * GPUs does not allow dynamic memory allocation. So, we have
         * to set X on host first.
         */
        host_x.coeffRef(aInitialNode, 0) = true;

        return BfsCalc_(host_x, aBfsVisit, nullptr);
    }

    /*! \brief Overload Bfs_ for the special case of starting from the inital
     * state
     * \details Executes a breadth-first search on the graph starting from the
     * system's initial node The algorithm is based on SpGEMM.
     *
     * \return Return a set containing the accessed states, or nullptr if none
     * was accessed or a bfs visit function was defined
     */
    StatesSet* Bfs_() const { return Bfs_(this->init_state_, nullptr); }

    /*! \brief Calculates Bfs and returns accessed states array
     * \details Executes a breadth first search on the graph starting from one
     * single node. The algorithm is based on SpGEMM.
     *
     * @param aHostX Bit vector with initial nodes as nonzeros rows.
     * @param aBfsVisit Function to execute on every accessed state: it can be
     * null
     * @param aStatesMap Map each vector used to implement in a bfs when
     * multiple ones are execute togheter: it can be null
     */
    StatesSet* BfsCalc_(
      StatesVector& aHostX,
      std::function<void(StorageIndex const&, StorageIndex const&)> const&
        aBfsVisit,
      std::vector<StorageIndex> const* const aStatesMap) const
    {
        auto n_initial_nodes = aHostX.cols();

        /*!
         * BFS on a Linear Algebra approach:
         *     \f$Y = G^T * X\f$
         */
        StatesVector y{ static_cast<StorageIndexSigned>(this->states_number_),
                        static_cast<StorageIndexSigned>(n_initial_nodes) };
        auto n_accessed_states = 0l;
        for (StorageIndex i = 0ul; i < this->states_number_; ++i) {
            y = bit_graph_ * aHostX;

            if (n_accessed_states == y.nonZeros()) {
                break;
            } else {
                n_accessed_states = y.nonZeros();
            }

            aHostX = y;
        }

        /// Add results to a std::set vector
        if (aBfsVisit) {
            for (auto s = 0l; s < y.rows(); ++s) {
                for (ColIteratorConst e(y, s); e; ++e) {
                    aBfsVisit((*aStatesMap)[e.col()], e.row());
                }
            }
            return nullptr;
        }

        auto accessed_states = new StatesSet[n_initial_nodes];
        for (auto s = 0l; s < y.cols(); ++s) {
            for (ColIteratorConst e(y, s); e; ++e) {
                accessed_states[e.col()].emplace(e.row());
            }
        }
        return accessed_states;
    }

private:
    friend class TransitionProxy<NEvents, StorageIndex>;

    /*! \brief DESystem operations virtual proxies
     *
     * Functions which implement DES operations between two systems and need
     * to access private members of DESystem. The design constrains that lead to
     * declare the following friend(s):
     *
     * * Lazy evaluation
     * * Functions which have more than one system as input and returns a
     * different system should not be a member of DESystem class.
     * * More efficiency when accessing vars than using getters and setters.
     * * Define a different namespace for DES operations.
     */
    friend class op::SyncSysProxy<NEvents, StorageIndex>;

    /*! \brief Graph represented by an adjacency matrix of bitsets
     * \details A sparse matrix which represents the automata as a graph in an
     * adjascency matrix. It is implemented as a CSR scheme.
     *  * Non zero element: events bit from <row index> to <col index> that
     * lead to starting node (row index) to the arriving node (col index).
     * * e.g. M(2, 3) = 101; Transition from state 2 to state 3 with the condition
     * event 0 OR event 2.
     */
    GraphHostData graph_;

    /*! \brief Graph of bit represented by an adjacency matrix
     * \details A sparse bit matrix which each non zero elem represents that a state
     * has at least one transition to other state.
     * It is used to calculate the accessible part, coaccessible part and trim
     * operations efficiently.
     * The matrix is also transposed and added to the identity in order to
     * make the accessible part op more efficient:
     * when calculating trim states, it is always necessary to calculate the
     * accessible part first. It implies that the accessible part usually is
     * calculated with a larger matrix. Adding the identity avoid to add two
     * sparse matrices each bfs iteration, which is inneficient.
     */
    BitGraphHostData bit_graph_;

    /*! \brief Inverted graph shared pointer
     * \details Used for searching inverted transitions when necessary.
     * Keep it free whenever it is possible.
     * TODO: Make unique
     */
    std::shared_ptr<GraphHostData> mutable inv_graph_;

    /*! \brief Keeps if caching graph data on device is enabled
     * \details If dev_cache_enabled_ is true, the graph should be cached on
     * the device memory, so device_graph_ is not nullptr. It can be set at any
     * time at run time, so it is not a constant.
     * \warning The OpenCL functions are disabled.
     */
    bool dev_cache_enabled_;

    /*! \brief Keeps track if the device graph cache is outdated
     * \details  Tracks if cache needs to be updated or not.
     * \warning The OpenCL functions are disabled.
     */
    bool is_cache_outdated_;
};

} // namespace cldes

// include transition proxy class
#include "cldes/TransitionProxy.hpp"

// Matrix proxy for sync operation
#include "cldes/operations/SyncSysProxy.hpp"

#endif // DESYSTEM_HPP
