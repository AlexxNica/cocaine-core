/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_CHAMBER_HPP
#define COCAINE_CHAMBER_HPP

#include "cocaine/common.hpp"

#include <boost/asio/io_service.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/thread/thread.hpp>

namespace cocaine { namespace io {

class chamber_t {
    const std::string name;
    const std::shared_ptr<boost::asio::io_service> asio;

    // Keeps reactor alive.
    std::unique_ptr<const boost::asio::io_service::work> work;

    // This thread will run the reactor's event loop until terminated.
    std::unique_ptr<boost::thread> thread;

    class named_runnable_t;

public:
    chamber_t(const std::string& name, const std::shared_ptr<boost::asio::io_service>& asio);
   ~chamber_t();
};

}} // namespace cocaine::io

#endif
