#ifndef SQLGUARD_EXPORT_H
#define SQLGUARD_EXPORT_H

/*
 * Shared-library visibility for the public SQLGuard API.
 * The static target publishes SQLGUARD_STATIC, so the same headers work
 * without dllimport/dllexport noise on Windows.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(SQLGUARD_STATIC)
#define SQLGUARD_API
#elif defined(SQLGUARD_BUILDING_LIBRARY)
#define SQLGUARD_API __declspec(dllexport)
#else
#define SQLGUARD_API __declspec(dllimport)
#endif
#elif defined(SQLGUARD_BUILDING_LIBRARY) && defined(__GNUC__)
#define SQLGUARD_API __attribute__((visibility("default")))
#else
#define SQLGUARD_API
#endif

#endif
