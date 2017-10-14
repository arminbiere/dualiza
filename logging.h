#ifndef NLOG
void log_message (const char *, ...);
void log_clause (Clause *, const char *, ...);
#define LOG(FMT,ARGS...) \
do { \
  if (!logging) break; \
  log_message (FMT, ##ARGS); \
} while (0)
#define LOGCLS(C,FMT,ARGS...) \
do { \
  if (!logging) break; \
  log_clause (C, FMT, ##ARGS); \
} while (0)
#else
#define LOG(ARGS...) do { } while (0)
#define LOGCLS(ARGS...) do { } while (0)
#endif
