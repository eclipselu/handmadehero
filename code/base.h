#ifndef BASE_H

#define PI 3.14159265358979323846

#define global        static
#define local_persist static
#define internal      static

typedef float  float32_t;
typedef double float64_t;

// utility macros
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

#define BASE_H
#endif
