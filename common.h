#ifndef _COMMON_H_
#define _COMMON_H_
//#define _XOPEN_SOURCE 500
#include <stdio.h>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <float.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STDFS
#define STDFS stderr
#endif

#define MESSAGE(fs,lvl,prefix,...)  do{fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\n");fflush(fs);}while(0)
#define MESSAGEn(fs,lvl,prefix,...) do{fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);                  fflush(fs);}while(0)

#define fprint_debug(fs,...)   MESSAGE  (fs, 5, "debug: ",   __VA_ARGS__)
#define fprint_status(fs,...)  MESSAGE  (fs, 4, "status: ",  __VA_ARGS__)
#define fprint_info(fs,...)    MESSAGE  (fs, 3, "info: ",    __VA_ARGS__)
#define fprint_warning(fs,...) MESSAGE  (fs, 2, "warning: ", __VA_ARGS__)
#define fprint_error(fs,...)   MESSAGE  (fs, 1, "error: ",   __VA_ARGS__)
#define fprint_abort(fs,...)   MESSAGE  (fs, 1, "abort: ",   __VA_ARGS__)
#define fprint(fs,...)         MESSAGE  (fs, 0, "",  __VA_ARGS__)
#define fprintn(fs,...)        MESSAGEn (fs, 0, "",  __VA_ARGS__)

#define myexit() pthread_exit(NULL);
#define fexit_debug(fs,...)    do {fprint_debug   (fs, __VA_ARGS__); myexit (); } while (0)
#define fexit_info(fs,...)     do {fprint_info    (fs, __VA_ARGS__); myexit (); } while (0)
#define fexit_warning(fs,...)  do {fprint_warning (fs, __VA_ARGS__); myexit (); } while (0)
#define fexit_error(fs,...)    do {fprint_error   (fs, __VA_ARGS__); myexit (); } while (0)
#define fexit_abort(fs,...)    do {fprint_abort   (fs, __VA_ARGS__); myexit (); } while (0)

#define print_debug(...)       fprint_debug   (STDFS, __VA_ARGS__)
#define print_warning(...)     fprint_warning (STDFS, __VA_ARGS__)
#define print_error(...)       fprint_error   (STDFS, __VA_ARGS__)
#define print(...)             fprint         (STDFS, __VA_ARGS__)
#define printn(...)            fprintn        (STDFS, __VA_ARGS__)

#define exit_debug(...)        fexit_debug    (STDFS, __VA_ARGS__)
#define exit_error(...)        fexit_error    (STDFS, __VA_ARGS__)
#define exit_abort(...)        fexit_abort    (STDFS, __VA_ARGS__)
#define exit_warning(...)      fexit_warning  (STDFS, __VA_ARGS__)
#define exit_status(...)       fexit_status   (STDFS, __VA_ARGS__)
#define exit_info(...)         fexit_info     (STDFS, __VA_ARGS__)

#define equal(foo,bar) (strcmp(foo,bar)==0)
#define catch_nan(x) if (isnan(x)){exit_error ("caught nan! %s", #x);}

#define foobar print_debug ("foobar")
#define true_or_exit_error(test,...) if(!(test))exit_error(__VA_ARGS__)

void *safe_malloc (size_t size);
void *safe_calloc (size_t num, size_t size);

double date_string_to_double (char* str);
char* double_to_date_string (double uClock);
void copy_file (char* from, char* to);
int file_exists (const char* file);
char* read_file (const char* file);
double get_time (void);
long get_time_us (void);
void print_remaining_time (double startTime, double tsp, double doneRatio, int counter);
char *parse_csv (char *str, int *argc, char ***argv, char sep, int inplace);
double get_sorted_value_at_index (double *list, int n, int j);

void internal_quicksort  (void *buf, size_t itemSize, size_t offset, int first, int last, int ascending);

enum {SORT_MODE_DESCENDING, SORT_MODE_ASCENDING};
#define quicksort_double(buf,len,ascending) internal_quicksort (buf, sizeof ((buf)[0]), 0, 0, len-1, ascending);
#define quicksort_structd(buf,element,len,ascending) internal_quicksort (buf, sizeof ((buf)[0]), (uint8_t *) & (buf)->element - (uint8_t *) (buf), 0, len-1, ascending);

#define SIGN(x) (((x) > 0) - ((x) < 0))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) > 0) ? (a) : -(a))
#define POW2(x) ((x) * (x))
#define POW3(x) ((x) * (x) * (x))
#define POW4(x) ((x) * (x) * (x) * (x))
#define TRUNCATE(x,min,max) (((x) < (min)) ? (min) : ((x) > (max)) ? (max) : (x))
#define NaN (0.0 / 0.0)

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _COMMON_H_ */
