#include "headers.h"

#include <signal.h>

static volatile int catched_signal;

#define SIGNALS \
SIGNAL(SIGINT) \
SIGNAL(SIGSEGV) \
SIGNAL(SIGABRT) \
SIGNAL(SIGTERM) \

#define SIGNAL(SIG) \
static void (*SIG ## _handler)(int);
SIGNALS
#undef SIGNAL

void reset_signal_handlers () {
#define SIGNAL(SIG) \
  (void) signal (SIG, SIG ## _handler);
  SIGNALS
#undef SIGNAL
  catched_signal = 0;
}

static const char * signal_name (int sig) {
#define SIGNAL(SIG) \
  if (sig == SIG) return # SIG; else
  SIGNALS
#undef SIGNAL
  return "UNKNOWN";
}

static void catch_signal (int sig) {
  if (!catched_signal) {
    catched_signal = 1;
    msg (1, "");
    msg (1, "caught signal(%d) '%s'", sig, signal_name (sig));
    print_rules ();
    print_statistics ();
  }
  msg (1, "reraising signal(%d) '%s'", sig, signal_name (sig));
  reset_signal_handlers ();
  raise (sig);
}

void set_signal_handlers () {
#define SIGNAL(SIG) \
  SIG ## _handler = signal (SIG, catch_signal);
  SIGNALS
#undef SIGNAL
}

