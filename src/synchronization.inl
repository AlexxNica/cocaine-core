/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"

struct context_t::synchronization_t:
    public basic_slot<io::locator::synchronize>
{
    typedef io::event_traits<io::locator::synchronize>::result_type upstream_type;

    synchronization_t(context_t& self);

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const std::shared_ptr<upstream_t>& upstream);

    void
    announce();

    void
    shutdown();

private:
    auto
    dump() const -> upstream_type;

private:
    context_t& self;

    // Remote clients for future updates.
    std::vector<std::shared_ptr<upstream_t>> upstreams;
};

context_t::synchronization_t::synchronization_t(context_t& self_):
    self(self_)
{ }

std::shared_ptr<dispatch_t>
context_t::synchronization_t::operator()(const msgpack::object& /* unpacked */, const std::shared_ptr<upstream_t>& upstream) {
    upstream->send<io::streaming<upstream_type>::chunk>(dump());

    // Save this upstream for the future notifications.
    upstreams.push_back(upstream);

    // Return an empty protocol dispatch.
    return std::shared_ptr<dispatch_t>();
}

void
context_t::synchronization_t::announce() {
    upstream_type value = dump();

    for(auto it = upstreams.begin(); it != upstreams.end(); ++it) {
        (*it)->send<io::streaming<upstream_type>::chunk>(value);
    }
}

void
context_t::synchronization_t::shutdown() {
    for(auto it = upstreams.begin(); it != upstreams.end(); ++it) {
        (*it)->seal<io::streaming<upstream_type>::choke>();
    }

    upstreams.clear();
}

auto
context_t::synchronization_t::dump() const -> upstream_type {
    upstream_type value;

    std::lock_guard<std::mutex> guard(self.m_mutex);

    for(auto it = self.m_services.begin(); it != self.m_services.end(); ++it) {
        value[it->first] = it->second->metadata();
    }

    return value;
}
