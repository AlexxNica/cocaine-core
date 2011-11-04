#ifndef COCAINE_DRIVERS_TIMED_HPP
#define COCAINE_DRIVERS_TIMED_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

template<class T>
class timed_t:
    public driver_t
{
    public:
        timed_t(engine_t* engine, const std::string& method):
            driver_t(engine, method)
        {
            m_watcher.set(this);
            ev_periodic_set(static_cast<ev_periodic*>(&m_watcher), 0, 0, thunk);
            m_watcher.start();
        }

    public: 
        virtual void suspend() {
            m_watcher.stop();
        }

        virtual void resume() {
            m_watcher.start();
        }

        void operator()(ev::periodic&, int) {
            boost::shared_ptr<lines::publication_t> deferred(new lines::publication_t(this));

            try {
                deferred->enqueue();
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                    m_engine->name().c_str(), m_method.c_str(), e.what());
                deferred->abort(lines::deferred_t::server_error, e.what());
            }
        }
    private:
        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            return static_cast<T*>(w->data)->reschedule(now);
        }

    private:
        ev::periodic m_watcher;
};

}}}

#endif
