#ifndef POLLER_INCLUDED
# define POLLER_INCLUDED

#include <network/socket_handler.hpp>

#include <unordered_map>
#include <memory>

#define POLL 1
#define EPOLL 2
#define KQUEUE 3
#include <config.h>
#ifndef POLLER
 #define POLLER POLL
#endif

#if POLLER == POLL
 #include <poll.h>
 #define MAX_POLL_FD_NUMBER 4096
#elif POLLER == EPOLL
  #include <sys/epoll.h>
#else
  #error Invalid POLLER value
#endif

/**
 * We pass some SocketHandlers to this Poller, which uses
 * poll/epoll/kqueue/select etc to wait for events on these SocketHandlers,
 * and call the callbacks when event occurs.
 *
 * TODO: support these pollers:
 * - kqueue(2)
 */

class Poller
{
public:
  explicit Poller();
  ~Poller();
  /**
   * Add a SocketHandler to be monitored by this Poller. All receive events
   * are always automatically watched.
   */
  void add_socket_handler(std::shared_ptr<SocketHandler> socket_handler);
  /**
   * Remove (and stop managing) a SocketHandler, designed by the given socket_t.
   */
  void remove_socket_handler(const socket_t socket);
  /**
   * Signal the poller that he needs to watch for send events for the given
   * SocketHandler.
   */
  void watch_send_events(SocketHandler* socket_handler);
  /**
   * Signal the poller that he needs to stop watching for send events for
   * this SocketHandler.
   */
  void stop_watching_send_events(SocketHandler* socket_handler);
  /**
   * Wait for all watched events, and call the SocketHandlers' callbacks
   * when one is ready.
   * Returns false if there are 0 SocketHandler in the list.
   */
  bool poll();

private:
  /**
   * A "list" of all the SocketHandlers that we manage, indexed by socket,
   * because that's what is returned by select/poll/etc when an event
   * occures.
   */
  std::unordered_map<socket_t, std::shared_ptr<SocketHandler>> socket_handlers;

#if POLLER == POLL
  struct pollfd fds[MAX_POLL_FD_NUMBER];
  nfds_t nfds;
#elif POLLER == EPOLL
  int epfd;
#endif

  Poller(const Poller&) = delete;
  Poller(Poller&&) = delete;
  Poller& operator=(const Poller&) = delete;
  Poller& operator=(Poller&&) = delete;
};

#endif // POLLER_INCLUDED
