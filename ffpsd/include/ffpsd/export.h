#ifndef FFPSD_EXPORT_H_
#define FFPSD_EXPORT_H_

#if defined(FFPSD_STATIC)
#define FFPSD_EXPORT
#define FFPSD_NO_EXPORT

#elif defined(_WIN32)
#ifdef FFPSD_BUILDING
#define FFPSD_EXPORT __declspec(dllexport)
#else
#define FFPSD_EXPORT __declspec(dllimport)
#endif
#define FFPSD_NO_EXPORT

#else // GCC / Clang
#define FFPSD_EXPORT __attribute__((visibility("default")))
#define FFPSD_NO_EXPORT __attribute__((visibility("hidden")))
#endif

#endif // FFPSD_EXPORT_H_