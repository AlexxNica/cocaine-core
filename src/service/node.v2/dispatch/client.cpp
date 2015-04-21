#include "cocaine/detail/service/node.v2/dispatch/client.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

streaming_dispatch_t::streaming_dispatch_t(const std::string& name):
    dispatch<tag>(format("C => W(%s)", name)),
    attached(false),
    closed(false)
{
    on<protocol::chunk>([&](const std::string& chunk){
        stream.write(chunk);
    });

    on<protocol::error>([&](int id, const std::string& reason){
        stream.abort(id, reason);

        std::lock_guard<std::mutex> lock(mutex);
        BOOST_ASSERT(!closed);

        closed = true;
        if (attached) {
            callback();
        }
    });

    on<protocol::choke>([&]{
        stream.close();

        std::lock_guard<std::mutex> lock(mutex);
        BOOST_ASSERT(!closed);

        closed = true;
        if (attached) {
            callback();
        }
    });
}

void
streaming_dispatch_t::attach(std::shared_ptr<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> stream,
                             std::function<void()> callback) {
    this->stream.attach(std::move(*stream));

    std::lock_guard<std::mutex> lock(mutex);
    if (closed) {
        callback();
    } else {
        attached = true;
        this->callback = callback;
    }
}
