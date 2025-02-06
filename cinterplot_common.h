#ifndef _CIP_COMMON_H_
#define _CIP_COMMON_H_
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

#define LOG_LEVEL_DEBUG   5
#define LOG_LEVEL_STATUS  4
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_NONE    0

#ifndef logLevel
#define logLevel LOG_LEVEL_DEBUG
#endif


#define MESSAGE(fs,lvl,prefix,...)  do{fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\n");fflush(fs);}while(0)
#define MESSAGEn(fs,lvl,prefix,...) do{fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);                  fflush(fs);}while(0)
#define MESSAGEcx(fs,lvl,col,...)         do{if(lvl<=logLevel) {fprintf (fs, "\033[0;%dm",col);fprintf (fs,__VA_ARGS__);fprintf(fs, "\033[0m"); fflush(fs);}}while(0)

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

#define ASCII_RED         31
#define ASCII_GREEN       32
#define ASCII_YELLOW      33
#define ASCII_BLUE        34
#define ASCII_PURPLE      35
#define ASCII_CYAN        36
#define ASCII_GRAY        37

#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE   "\033[0;34m"
#define COLOR_PURPLE "\033[0;35m"
#define COLOR_CYAN   "\033[0;36m"
#define COLOR_GRAY   "\033[0;37m"
#define COLOR_END    "\033[0m"

#define fprint_col(fs,col,...)  MESSAGEcx  (fs, 0, col,  __VA_ARGS__)

#define fprint_red(fs,...)         fprint_col     (fs, ASCII_RED   , __VA_ARGS__)
#define fprint_green(fs,...)       fprint_col     (fs, ASCII_GREEN , __VA_ARGS__)
#define fprint_yellow(fs,...)      fprint_col     (fs, ASCII_YELLOW, __VA_ARGS__)
#define fprint_blue(fs,...)        fprint_col     (fs, ASCII_BLUE  , __VA_ARGS__)
#define fprint_purple(fs,...)      fprint_col     (fs, ASCII_PURPLE, __VA_ARGS__)
#define fprint_cyan(fs,...)        fprint_col     (fs, ASCII_CYAN  , __VA_ARGS__)
#define fprint_gray(fs,...)        fprint_col     (fs, ASCII_GRAY  , __VA_ARGS__)

#define print_red(...)         fprint_red     (STDFS, __VA_ARGS__)
#define print_green(...)       fprint_green   (STDFS, __VA_ARGS__)
#define print_yellow(...)      fprint_yellow  (STDFS, __VA_ARGS__)
#define print_blue(...)        fprint_blue    (STDFS, __VA_ARGS__)
#define print_purple(...)      fprint_purple  (STDFS, __VA_ARGS__)
#define print_cyan(...)        fprint_cyan    (STDFS, __VA_ARGS__)
#define print_gray(...)        fprint_gray    (STDFS, __VA_ARGS__)

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

#endif /* _CIP_COMMON_H_ */
