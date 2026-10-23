#include "../../scaler.h"
#include "xbrz.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" const Scaler EXPORT *getScaler();
