/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_RAFT_SERVICE_HPP
#define COCAINE_RAFT_SERVICE_HPP

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/api/service.hpp"
#include "cocaine/idl/raft.hpp"
#include "cocaine/detail/raft/forwards.hpp"

namespace cocaine { namespace raft {

// The service provides messages from other Raft nodes to proper Raft actors.
// Protocol is defined in cocaine/idl/raft.hpp.
class service_t:
    public api::service_t,
    public implements<io::raft_tag<msgpack::object, msgpack::object>>
{
    typedef actor<configuration_machine_t, cocaine::raft::configuration<configuration_machine_t>>
            config_actor_type;

public:
    service_t(context_t& context,
              io::reactor_t& reactor,
              const std::string& name);

    virtual
    auto
    prototype() -> dispatch_t& {
        return *this;
    }

    const std::shared_ptr<config_actor_type>&
    configuration_actor() const;

private:
    std::shared_ptr<raft::actor_concept_t>
    find_machine(const std::string& name) const;

    deferred<std::tuple<uint64_t, bool>>
    append(const std::string& state_machine,
           uint64_t term,
           raft::node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index);

    deferred<std::tuple<uint64_t, bool>>
    apply(const std::string& state_machine,
          uint64_t term,
          raft::node_id_t leader,
          std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
          const msgpack::object& snapshot,
          uint64_t commit_index);

    deferred<std::tuple<uint64_t, bool>>
    request_vote(const std::string& state_machine,
                 uint64_t term,
                 raft::node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry);

    deferred<command_result<void>>
    insert_internal(const std::string& machine, const node_id_t& node);

    deferred<command_result<void>>
    erase_internal(const std::string& machine, const node_id_t& node);

    deferred<command_result<cluster_change_result>>
    insert(const std::string& machine, const node_id_t& node);

    deferred<command_result<cluster_change_result>>
    erase(const std::string& machine, const node_id_t& node);

    deferred<command_result<void>>
    lock(const std::string& machine);

    deferred<command_result<void>>
    reset(const std::string& machine, const cluster_config_t& new_config);

    void
    on_config_void_result(deferred<command_result<void>> promise,
                          const std::error_code& ec);

    void
    on_config_change_error(const std::string& machine,
                           uint64_t operation_id,
                           deferred<command_result<cluster_change_result>> promise,
                           const std::error_code& ec);

    void
    on_config_change_result(deferred<command_result<cluster_change_result>> promise,
                            const boost::variant<std::error_code, cluster_change_result>& result);

private:
    context_t& m_context;

    io::reactor_t& m_reactor;

    std::unique_ptr<logging::log_t> m_log;

    std::shared_ptr<config_actor_type> m_config_actor;
};

}} // namespace cocaine::service

#endif // COCAINE_RAFT_SERVICE_HPP
