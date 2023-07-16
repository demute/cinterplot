#include "common.h"
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <math.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>


void *safe_malloc (size_t size)
{
    void *ptr = malloc (size);
    if (!ptr)
        exit_error ("can't malloc %ld", size);
    return ptr;
}

void *safe_calloc (size_t count, size_t size)
{
    void *ptr = calloc (count, size);
    if (!ptr)
        exit_error ("can't calloc %ld %ld", count, size);
    return ptr;
}

double date_string_to_double (char* str)
{

    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_isdst = 1;
    time_t thetime;
    if (strptime (str, "%Y-%m-%d %H:%M:%S", & tm) != NULL)
        thetime = mktime (&tm);
    else
        exit_error ("failed to extract time");
    return (double) thetime;
}

char* double_to_date_string (double uClock)
{
    long uClockSecs   = (long) uClock;
    double uClockFrac = uClock - (double) uClockSecs;
    time_t uc    = uClockSecs;
    struct tm* t = localtime (& uc);

    static char buf[128];
    char fracbuf[32];
    strftime (buf, sizeof (buf), "%Y-%m-%d/%H:%M:%S", t);
    snprintf (fracbuf, sizeof (fracbuf), "%09.6f", uClockFrac);
    strcat (buf, & fracbuf[2]);
    return buf;
}


char *parse_csv (char *str, int *argc, char ***argv, char sep, int inplace)
{
    if (!str)
        return NULL;

    char *modStr = inplace ? str : strdup (str);
    assert (modStr);

    int cnt = 1;
    int len = (int) strlen (modStr);
    for (int i=0; i<len; i++)
        if (modStr[i] == sep)
            cnt++;

    char **strings = (char**) malloc ((uint32_t) cnt * sizeof (char*));
    assert (strings);


    int argi=0, i=0;
    strings[argi++] = modStr;
    while (modStr[i])
    {
        if (modStr[i] == sep)
        {
            modStr[i++] = '\0';
            strings[argi++] = & modStr[i];
        }
        else
        {
            i++;
        }
    }

    *argc = cnt;
    *argv = strings;

    return modStr;
}

void copy_file (char* from, char* to)
{
    FILE* src = fopen (from, "r");
    assert (src);

    fseek (src, 0L, SEEK_END);
    size_t size = (size_t) ftell (src);
    fseek (src, 0L, SEEK_SET);

    char* buf = (char*) malloc (sizeof (char) * size);
    assert (buf);

    if (fread  (buf, sizeof (char), size, src) != size)
        exit_error ("fread did not return expected size");
    fclose (src);

    FILE* dst = fopen (to, "w");
    assert (dst);
    if (fwrite (buf, sizeof (char), size, dst) != size)
        exit_error ("fwrite did not return expected size");

    free (buf);
    fclose (dst);
}

int file_exists (const char* file)
{
    struct stat s;
    return stat (file, & s) == 0;
}

char* read_file (const char* file)
{
    if (!file_exists (file))
        exit_error ("can't find %s", file);
    FILE* fs = fopen (file, "r");
    fseek (fs, 0L, SEEK_END);
    size_t fsize = (size_t) ftell (fs);
    rewind (fs);
    char* buffer = (char*) malloc (sizeof (char) * (fsize + 1));
    assert (buffer);
    if (fread (buffer, sizeof (char), fsize, fs) != fsize)
        exit_error ("could not read fsize=%lu from file %s", fsize, file);
    buffer[fsize] = '\0';
    fclose (fs);

    return buffer;
}

long get_time_us (void)
{
    struct timeval t;
    gettimeofday (& t, 0);
    long timestamp = t.tv_sec*1000000 + t.tv_usec;
    return timestamp;
}

double get_time (void)
{
    struct timeval t;
    gettimeofday (& t, 0);
    double timestamp = (double) t.tv_sec + (double) t.tv_usec / 1000000;
    return timestamp;
}

void print_remaining_time (double startTime, double tsp, double doneRatio, int counter)
{
    static int lastCounter = 0;
    static double lastTime = 0;
    static double lastPrinted = 0;
    double currentTime = get_time ();

    if (lastCounter && currentTime - lastPrinted > 1.0)
    {
        double countsPerSec = (double) (counter - lastCounter) / (currentTime - lastTime);
        fprintf (STDFS, "%s counter:%9d, (%7.0lf counts/sec",
                double_to_date_string (tsp), counter, countsPerSec);

        if (doneRatio != INFINITY && doneRatio != -INFINITY)
        {
            int secondsLeft = (int) ((currentTime - startTime) * ((1-doneRatio) / doneRatio));
            int minutesLeft = secondsLeft / 60;
            int hoursLeft   = minutesLeft / 60;
            secondsLeft    -= minutesLeft * 60;
            minutesLeft    -= hoursLeft   * 60;

            fprintf (STDFS, ", %5.2lf %% done, %02d:%02d:%02d remaining",
                    doneRatio * 100, hoursLeft, minutesLeft, secondsLeft);
        }
        fprintf (STDFS, ")\n");
        lastPrinted = currentTime;
    }
    lastCounter = counter;
    lastTime    = currentTime;
}

#define CMPVAL(buf,itemSize,offset,i) (*(double *) ((uint8_t *) buf + i * itemSize + offset))
#define SWAP(buf,itemSize,a,b) do { \
    uint8_t *aptr=(uint8_t *) buf + a*itemSize; \
    uint8_t *bptr=(uint8_t *) buf + b*itemSize; \
    for(size_t i=0;i<itemSize;i++) \
    { \
        aptr[i] ^= bptr[i]; \
        bptr[i] ^= aptr[i]; \
        aptr[i] ^= bptr[i]; \
    } \
} while (0)

void internal_quicksort (void *buf, size_t itemSize, size_t offset, uint32_t first, uint32_t last, int ascending)
{
    if (first >= last)
        return;

    uint32_t pivot = first;
    uint32_t i = first;
    uint32_t j = last;

    while (i < j)
    {
        if (ascending)
        {
            while (CMPVAL (buf,itemSize,offset,i) <= CMPVAL (buf,itemSize,offset,pivot) && i < last)
                i++;
            while (CMPVAL (buf,itemSize,offset,j) > CMPVAL (buf,itemSize,offset,pivot))
                j--;
        }
        else
        {
            while (CMPVAL (buf,itemSize,offset,i) >= CMPVAL (buf,itemSize,offset,pivot) && i < last)
                i++;
            while (CMPVAL (buf,itemSize,offset,j) < CMPVAL (buf,itemSize,offset,pivot))
                j--;
        }
        if (i < j)
            SWAP (buf,itemSize,i,j);
    }

    if (pivot != j)
        SWAP (buf,itemSize,pivot,j);

    internal_quicksort  (buf, itemSize, offset, first, j-1, ascending);
    internal_quicksort  (buf, itemSize, offset, j+1, last, ascending);
}

double get_sorted_value_at_index (double *list, int n, int j)
{
    //printf ("list pre:  [");
    //for (int i=0; i<n; i++)
    //    printf ("%0.0f ", list[i]);
    //printf ("], index %d\n", j);


    double pivot = list[n / 2];
    //printf ("pivot: %0.0f\n", pivot);

    int r = n - 1;
    for (int i=0; i<n; i++)
    {
        if (list[i] >= pivot)
        {
            while (r > i)
            {
                if (list[r] < pivot)
                    break;
                r--;
            }

            if (r == i)
                break;

            double tmp = list[i];
            list[i] = list[r];
            list[r] = tmp;
        }
    }

    int s = r;
    r = n - 1;
    for (int i=s; i<n; i++)
    {
        if (list[i] > pivot)
        {
            while (r > i)
            {
                if (list[r] == pivot)
                    break;
                r--;
            }

            if (r == i)
                break;

            double tmp = list[i];
            list[i] = list[r];
            list[r] = tmp;
        }
    }


    //printf ("list post: [");
    //for (int i=0; i<n; i++)
    //    printf ("%0.0f ", list[i]);
    //printf ("], index %d\n", j);

    int a=0, b=0, c=0;
    for (int i=0; i<n; i++)
    {
        if (list[i] < pivot)
            a++;
        else if (list[i] == pivot)
            b++;
        else
            c++;
    }
    (void) c;

    // a = number of items strictly less
    // b = number of items strictly equal
    // c = number of items strictly greater

    // pivot has indices k between a <= k < a+b
    // if j is a member of this region, we return

    //printf ("a: %d b: %d c: %d\n", a, b, c);
    if (a <= j && j < a + b)
    {
        //printf ("success, returning\n");
        return pivot;
    }

    // otherwise if j < a, we know the value of index is smaller than pivot
    else if (j < a)
    {
        //printf ("recursing lower region\n");
        return get_sorted_value_at_index (list, a, j);
    }

    // otherwise j must be greater than or equal to a + b, in which case we find the index in this region
    else
    {
        //printf ("recursing higher region");
        return get_sorted_value_at_index (& list[a + b], n - (a + b), j - (a + b));
    }
}
