/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <boost/format.hpp>

#include "cocaine/slave/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine;
using namespace cocaine::engine;

slave_t::slave_t(context_t& context, slave_config_t config):
    unique_id_t(config.uuid),
    m_context(context),
    m_log(m_context.log("app/" + config.app)),
    m_bus(m_context.io(), ZMQ_DEALER, config.uuid)
{
    m_bus.connect(
        (boost::format("ipc://%1%/%2%")
            % m_context.config.ipc_path
            % config.app
        ).str()
    );
    
    m_watcher.set<slave_t, &slave_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_processor.set<slave_t, &slave_t::process>(this);
    m_check.set<slave_t, &slave_t::check>(this);
    m_check.start();
    // m_pumper.set<slave_t, &slave_t::pump>(this);
    // m_pumper.start(0.005f, 0.005f);

    m_heartbeat_timer.set<slave_t, &slave_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    configure(config.app);
}

slave_t::~slave_t() { }

void slave_t::configure(const std::string& app) {
    try {
        m_manifest.reset(new manifest_t(m_context, app));
        
        m_sandbox = m_context.get<sandbox_t>(
            m_manifest->type,
            category_traits<sandbox_t>::args_type(*m_manifest)
        );
        
        m_suicide_timer.set<slave_t, &slave_t::timeout>(this);
        m_suicide_timer.start(m_manifest->policy.suicide_timeout);
    } catch(const configuration_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        m_bus.send(packed);
        terminate();
    } catch(const repository_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        m_bus.send(packed);
        terminate();
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        m_bus.send(packed);
        terminate();
    } catch(...) {
        rpc::packed<rpc::error> packed(
            dealer::server_error,
            "unexpected exception while configuring the slave"
        );
        
        m_bus.send(packed);
        terminate();
    }
}

void slave_t::run() {
    m_loop.loop();
}

blob_t slave_t::read(int timeout) {
    zmq::message_t message;

    io::scoped_option<
        io::options::receive_timeout
    > option(m_bus, timeout);

    m_bus.recv(&message);
    
    return blob_t(message.data(), message.size());
}

void slave_t::write(const void * data, size_t size) {
    rpc::packed<rpc::chunk> packed(data, size);
    m_bus.send(packed);
}

void slave_t::message(ev::io&, int) {
    if(m_bus.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void slave_t::process(ev::idle&, int) {
    if(!m_bus.pending()) {
        m_processor.stop();
        return;
    }
    
    int command = 0;

    m_bus.recv(command);

    switch(command) {
        case rpc::invoke: {
            // TEST: Ensure that we have the app first.
            BOOST_ASSERT(m_sandbox.get() != NULL);

            std::string event;

            m_bus.recv(event);
            invoke(event);
            
            break;
        }
        
        case rpc::terminate:
            terminate();
            break;

        default:
            m_log->warning(
                "slave %s dropping unknown event type %d", 
                id().c_str(),
                command
            );
            
            m_bus.drop();
    }

    // TEST: Ensure that we haven't missed something.
    BOOST_ASSERT(!m_bus.more());
}

void slave_t::check(ev::prepare&, int) {
    message(m_watcher, ev::READ);
}

// void slave_t::pump(ev::timer&, int) {
//     message(m_watcher, ev::READ);
// }

void slave_t::timeout(ev::timer&, int) {
    terminate();
}

void slave_t::heartbeat(ev::timer&, int) {
    rpc::packed<rpc::heartbeat> packed;
    m_bus.send(packed);
}

void slave_t::invoke(const std::string& event) {
    try {
        m_sandbox->invoke(event, *this);
    } catch(const recoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::app_error, e.what());
        m_bus.send(packed);
    } catch(const unrecoverable_error_t& e) {
        rpc::packed<rpc::error> packed(dealer::server_error, e.what());
        m_bus.send(packed);
    } catch(...) {
        rpc::packed<rpc::error> packed(
            dealer::server_error,
            "unexpected exception while processing an event"
        );
        
        m_bus.send(packed);
    }
    
    rpc::packed<rpc::choke> packed;
    m_bus.send(packed);
    
    // NOTE: Drop all the outstanding request chunks not pulled
    // in by the user code. Might have a warning here?
    m_bus.drop();

    m_suicide_timer.stop();
    m_suicide_timer.start(m_manifest->policy.suicide_timeout);
}

void slave_t::terminate() {
    rpc::packed<rpc::terminate> packed;
    m_bus.send(packed);
    m_loop.unloop(ev::ALL);
} 
