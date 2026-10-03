// Host harness: drives the mqtt core over a plain POSIX TCP server (test only).
#include "mqtt_broker.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>

using namespace esphome::mqtt_broker;

static uint32_t now_ms() {
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

struct L : Listener {
  void on_log(LogLevel lv, const std::string &m) override {
    printf("LOG[%d] %s\n", static_cast<int>(lv), m.c_str());
    fflush(stdout);
  }
  void on_publish(Session &from, const std::string &t, const uint8_t *, size_t len, uint8_t qos, bool) override {
    printf("EVT publish from=%u topic=%s len=%zu qos=%u\n", from.id, t.c_str(), len, qos);
    fflush(stdout);
  }
  void on_connect(Session &s) override { printf("EVT connect %u '%s'\n", s.id, s.client_id.c_str()); fflush(stdout); }
  void on_disconnect(Session &s) override { printf("EVT disconnect %u '%s'\n", s.id, s.client_id.c_str()); fflush(stdout); }
  void on_subscribe(Session &s, const std::string &f, uint8_t q) override {
    printf("EVT subscribe %u %s qos=%u\n", s.id, f.c_str(), q);
    fflush(stdout);
  }
};

int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : 18830;
  const char *user = argc > 2 ? argv[2] : "";
  const char *pass = argc > 3 ? argv[3] : "";
  L listener;
  Broker broker(&listener);
  BrokerOptions opt;
  opt.username = user;
  opt.password = pass;
  opt.max_packet_size = 8192;
  broker.set_options(opt);

  int ls = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  if (bind(ls, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0 || listen(ls, 8) != 0) {
    perror("bind/listen");
    return 1;
  }
  fcntl(ls, F_SETFL, O_NONBLOCK);
  printf("READY %d\n", port);
  fflush(stdout);

  std::map<uint32_t, int> fds;
  uint32_t last_pub = now_ms();
  while (true) {
    std::vector<pollfd> pfds;
    pfds.push_back({ls, POLLIN, 0});
    for (auto &sp : broker.sessions()) {
      short ev = POLLIN;
      if (sp->tx_pending() > 0) ev |= POLLOUT;
      pfds.push_back({fds[sp->id], ev, 0});
    }
    poll(pfds.data(), pfds.size(), 50);
    uint32_t now = now_ms();

    if (pfds[0].revents & POLLIN) {
      while (true) {
        sockaddr_in pa{};
        socklen_t pl = sizeof(pa);
        int c = accept(ls, reinterpret_cast<sockaddr *>(&pa), &pl);
        if (c < 0) break;
        fcntl(c, F_SETFL, O_NONBLOCK);
        Session *s = broker.create_session(now, "peer");
        fds[s->id] = c;
      }
    }

    std::vector<Session *> victims;
    for (auto &sp : broker.sessions()) {
      Session &s = *sp;
      int fd = fds[s.id];
      uint8_t buf[512];
      bool alive = true;
      while (alive) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
          if (!broker.feed(s, buf, n, now)) alive = false;
        } else if (n == 0) {
          alive = false;
          s.close_after_flush = true;
          break;
        } else {
          break;
        }
      }
      while (s.tx_pending() > 0) {
        ssize_t n = write(fd, s.tx.data() + s.tx_pos, s.tx_pending());
        if (n > 0) s.tx_consumed(n);
        else break;
      }
      if (!alive && s.tx_pending() == 0) s.dead = true;
      if (s.dead || (s.close_after_flush && s.tx_pending() == 0)) victims.push_back(&s);
    }
    broker.tick(now);
    for (auto &sp : broker.sessions()) if (sp->dead) { bool f=false; for (auto*v:victims) if (v==sp.get()) f=true; if(!f) victims.push_back(sp.get()); }
    for (Session *v : victims) {
      close(fds[v->id]);
      fds.erase(v->id);
      broker.remove_session(v);
    }

    if (now - last_pub > 1000) {
      last_pub = now;
      size_t n = broker.publish("hb/broker/tick", std::string("tick"));
      (void) n;
    }
  }
}
