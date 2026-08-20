#ifndef _BENCHMARK_H_
#define _BENCHMARK_H_

#define MAX_NUM_LINES 65536
#define MAX_NUM_CHECKPOINTS 1024

void benchmark_create (void);
void benchmark_destroy (void);
void benchmark_start (int lineNumber, char *str);
void benchmark_add_checkpoint (int lineNumber, char *str);
void benchmark_stop (void);
void benchmark_dump (void);

#ifdef ENABLE_BENCHMARKING
#define BENCHMARK_CREATE()            benchmark_create ()
#define BENCHMARK_START(str)          benchmark_start (__LINE__, str)
#define BENCHMARK_ADD_CHECKPOINT(str) benchmark_add_checkpoint (__LINE__, str)
#define BENCHMARK_STOP()              benchmark_stop ()
#define BENCHMARK_DUMP()              benchmark_dump ()
#else
#define BENCHMARK_CREATE()
#define BENCHMARK_START(str)
#define BENCHMARK_ADD_CHECKPOINT(str)
#define BENCHMARK_STOP()
#define BENCHMARK_DUMP()
#endif

#endif /* _BENCHMARK_H_ */

