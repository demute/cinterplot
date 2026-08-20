#include "cinterplot_common.h"
#include "benchmark.h"

#define nanosec() clock_gettime_nsec_np (CLOCK_MONOTONIC_RAW);

typedef struct BenchmarkCheckpoint
{
    uint64_t tsp;
    char     *str;
    uint64_t elapsed;
} BenchmarkCheckpoint;

typedef struct BenchmarkData
{
    int checkpointIdx;
    uint64_t startTsp;
    uint64_t stopTsp;
    BenchmarkCheckpoint checkpoints[MAX_NUM_CHECKPOINTS];
    int lineNumberToIdx[MAX_NUM_LINES];
    int isRunning;
} BenchmarkData;

static BenchmarkData *bench = NULL;

void benchmark_create (void)
{
    bench = calloc (1, sizeof (BenchmarkData));
    assert (bench);
}

void benchmark_destroy (void)
{
    if (bench)
        free (bench);
    bench = NULL;
}

void benchmark_start (int lineNumber, char *str)
{
    assert (bench);
    bzero (bench->lineNumberToIdx, sizeof (bench->lineNumberToIdx));
    bzero (bench->checkpoints,     sizeof (bench->checkpoints));

    bench->isRunning = 1;
    bench->checkpointIdx = 0;
    bench->checkpoints[0].tsp = nanosec ();
    bench->checkpoints[0].str = str;
}

void benchmark_add_checkpoint (int lineNumber, char *str)
{
    if (bench->lineNumberToIdx[lineNumber] == 0)
        bench->lineNumberToIdx[lineNumber] = bench->checkpointIdx + 1;

    int nextIdx = bench->lineNumberToIdx[lineNumber];

    uint64_t tsp = nanosec ();
    BenchmarkCheckpoint *cp = & bench->checkpoints[bench->checkpointIdx];
    cp->elapsed += tsp - cp->tsp;

    bench->checkpointIdx = nextIdx;
    bench->checkpoints[nextIdx].tsp = tsp;
    bench->checkpoints[nextIdx].str = str;
}

void benchmark_stop (void)
{
    if (!bench || !bench->isRunning)
    {
        //print_debug ("invalid call to stop");
        return;
    }

    bench->isRunning = 0;

    uint64_t tsp = nanosec ();
    BenchmarkCheckpoint *cp = & bench->checkpoints[bench->checkpointIdx];
    cp->elapsed += tsp - cp->tsp;
}

void benchmark_dump (void)
{
    if (!bench || bench->isRunning)
        print_debug ("dumping cancelled");

    int numCheckpoints = bench->checkpointIdx + 1;
    BenchmarkCheckpoint *cps = bench->checkpoints;

    int maxlen = 0;
    uint64_t totalElapsed = 0;
    for (int ci=0; ci<numCheckpoints; ci++)
    {
        totalElapsed += cps[ci].elapsed;

        char *str = cps[ci].str ? cps[ci].str : "(null)";
        int len = strlen (str);
        if (maxlen < len)
            maxlen = len;
    }

    double pcntConversionFactor = 100.0 / totalElapsed;

    for (int ci=0; ci<numCheckpoints; ci++)
    {
        char *str = cps[ci].str ? cps[ci].str : "(null)";
        int len = strlen (str);
        printf ("%s", str);
        for (int i=0; i<(maxlen+4-len); i++)
            putchar ('.');

        printf ("%6.2f\n", cps[ci].elapsed * pcntConversionFactor);
    }
    double fps = 1e9 / totalElapsed;
    printf ("fps: %0.2f\n", fps);
}

