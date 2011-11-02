#ifndef UTILS_H
#define UTILS_H

#define DEBUG
#ifdef DEBUG
#define WHERESTR "[file %s, line %d]"
#define WHEREARG __FILE__, __LINE__
#define d(...) (elog(DEBUG1, WHERESTR, WHEREARG), elog(DEBUG1, __VA_ARGS__))
#else
#define d(...)
#endif

#endif
