#ifndef BASE_H

    #define PI 3.14159265358979323846

    #define global        static
    #define local_persist static
    #define internal      static

typedef float  float32_t;
typedef double float64_t;

    #if defined(__clang__)
        #define COMPILER_CLANG 1
    #elif defined(_MSC_VER)
        #define COMPILER_MSVC 1
    #elif defined(__GNUC__) || defined(__GNUG__)
        #define COMPILER_GCC 1
    #endif

    // utility macros
    #define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

    #if COMPILER_MSVC
        #define Trap() __debugbreak()
    #elif COMPILER_CLANG || COMPILER_GCC
        #if __has_builtin(__builtin_trap)
            #define Trap() __builtin_trap()
        #else
            #define Trap() abort()
        #endif
    #else
        #define Trap()
    #endif

    #define AssertAlways(x)                                                                                            \
        do {                                                                                                           \
            if (!(x)) {                                                                                                \
                Trap();                                                                                                \
            }                                                                                                          \
        } while (0)
    #if BUILD_DEBUG
        #define Assert(x) AssertAlways(x)
    #else
        #define Assert(x) (void)(x)
    #endif

    // KB/MB/GB
    #define KiloBytes(val) ((val) * 1024)
    #define MegaBytes(val) (KiloBytes(val) * 1024)
    #define GigaBytes(val) (MegaBytes(val) * 1024)

    #define BASE_H
#endif
