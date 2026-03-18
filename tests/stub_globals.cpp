#include <csignal>

volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_sigpipe = 0;
