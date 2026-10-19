#pragma once

#if defined(sireflect_STATIC)
#    define SIREFLECT_API
#elif defined(_WIN32)
#    if defined(sireflect_EXPORTS)
#        define SIREFLECT_API __declspec(dllexport)
#    else
#        define SIREFLECT_API __declspec(dllimport)
#    endif
#elif defined(sireflect_EXPORTS)
#    define SIREFLECT_API __attribute__((visibility("default")))
#else
#    define SIREFLECT_API
#endif
