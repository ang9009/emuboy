#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <string.h>

// Taken from https://stackoverflow.com/questions/66202939/using-perror-with-arguments
#define PERRORF(FMT, ...) \
  fprintf(stderr, FMT ": %s\n", ##__VA_ARGS__, strerror(errno))

#ifdef DEBUG
#define DBG_PRINT(DEBUG, MSG, ...)   \
  if (debug) {                       \
    printf(MSG "\n", ##__VA_ARGS__); \
  }
#else
#define DBG_PRINT(MSG, ...) \
  do {                      \
  } while (0)
#endif

#endif