#include <louloulibs.h>
#ifdef CARES_FOUND

#include <network/dns_socket_handler.hpp>
#include <network/dns_handler.hpp>
#include <network/poller.hpp>

#include <utils/timed_events.hpp>

#include <algorithm>
#include <stdexcept>

DNSHandler DNSHandler::instance;

using namespace std::string_literals;
DNSHandler::DNSHandler()
{
  int ares_error;
  if ((ares_error = ::ares_library_init(ARES_LIB_INIT_ALL)) != 0)
    throw std::runtime_error("Failed to initialize c-ares lib: "s + ares_strerror(ares_error));
  struct ares_options options = {};
  // The default timeout values are way too high
  options.timeout = 1000;
  options.tries = 3;
  if ((ares_error = ::ares_init_options(&this->channel,
                                        &options,
                                        ARES_OPT_TIMEOUTMS|ARES_OPT_TRIES)) != ARES_SUCCESS)
    throw std::runtime_error("Failed to initialize c-ares channel: "s + ares_strerror(ares_error));
}

ares_channel& DNSHandler::get_channel()
{
  return this->channel;
}

void DNSHandler::destroy()
{
  this->socket_handlers.clear();
  ::ares_destroy(this->channel);
  ::ares_library_cleanup();
}

void DNSHandler::gethostbyname(const std::string& name, ares_host_callback callback,
                               void* data, int family)
{
  if (family == AF_INET)
    ::ares_gethostbyname(this->channel, name.data(), family,
                         callback, data);
  else
    ::ares_gethostbyname(this->channel, name.data(), family,
                         callback, data);
}

void DNSHandler::watch_dns_sockets(std::shared_ptr<Poller>& poller)
{
  fd_set readers;
  fd_set writers;

  FD_ZERO(&readers);
  FD_ZERO(&writers);

  int ndfs = ::ares_fds(this->channel, &readers, &writers);
  // For each existing DNS socket, see if we are still supposed to watch it,
  // if not then erase it
  this->socket_handlers.erase(
      std::remove_if(this->socket_handlers.begin(), this->socket_handlers.end(),
                     [&readers](const auto& dns_socket)
                     {
                       return !FD_ISSET(dns_socket->get_socket(), &readers);
                     }),
      this->socket_handlers.end());

  for (auto i = 0; i < ndfs; ++i)
    {
      bool read = FD_ISSET(i, &readers);
      bool write = FD_ISSET(i, &writers);
      // Look for the DNSSocketHandler with this fd
      auto it = std::find_if(this->socket_handlers.begin(),
                             this->socket_handlers.end(),
                             [i](const auto& socket_handler)
                             {
        return i == socket_handler->get_socket();
      });
      if (!read && !write)      // No need to read or write to it
        { // If found, erase it and stop watching it because it is not
          // needed anymore
          if (it != this->socket_handlers.end())
            // The socket destructor removes it from the poller
            this->socket_handlers.erase(it);
        }
      else            // We need to write and/or read to it
        { // If not found, create it because we need to watch it
          if (it == this->socket_handlers.end())
            {
              this->socket_handlers.emplace(this->socket_handlers.begin(),
                                            std::make_unique<DNSSocketHandler>(poller, i));
              it = this->socket_handlers.begin();
            }
          poller->add_socket_handler(it->get());
          if (write)
            poller->watch_send_events(it->get());
        }
    }
  // Cancel previous timer, if any.
  TimedEventsManager::instance().cancel("DNS timeout");
  struct timeval tv;
  struct timeval* tvp;
  tvp = ::ares_timeout(this->channel, NULL, &tv);
  if (tvp)
    {
      auto future_time = std::chrono::steady_clock::now() + std::chrono::seconds(tvp->tv_sec) + \
        std::chrono::microseconds(tvp->tv_usec);
      TimedEventsManager::instance().add_event(TimedEvent(std::move(future_time),
                                                          [this]()
             {
               for (auto& dns_socket_handler: this->socket_handlers)
                 dns_socket_handler->on_recv();
             },
                                                          "DNS timeout"));
    }
}

#endif /* CARES_FOUND */
