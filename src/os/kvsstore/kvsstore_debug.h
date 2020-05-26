#ifndef SRC_OS_KVSSTORE_KVSSTORE_DEBUG_H_
#define SRC_OS_KVSSTORE_KVSSTORE_DEBUG_H_

#define _KVSFILELOGGING
#include "kvsstore_debug_impl.h"

// common
#define _KVSLOGGER_IMPL(a,b) SimpleLoggerBuffer() << "[" << get_thread_name() << "," << std::setw(15)  << get_fixed_func_name(a) << "," << std::setw(5) << b << "] "
#define _KVSLOGGER _KVSLOGGER_IMPL(__FUNCTION__, __LINE__)
#define _KVSNULLLOGGER if (0) std::cout
#define TRERR _KVSLOGGER << "ERR: "
#define TRBACKTRACE { ostringstream oss; oss << BackTrace(1); _KVSLOGGER << "Backtrace: " << oss.str(); }

// LOG switches
#define TR   _KVSLOGGER
#define TR2  _KVSNULLLOGGER
#define TRI  _KVSNULLLOGGER
#define TRW  _KVSNULLLOGGER
#define TRR  _KVSNULLLOGGER
#define TRIO _KVSNULLLOGGER

#ifdef ENABLE_FTRACE
struct FtraceObject {
    std::string func;
    int line;
    FtraceObject(const char *f, int line_) : func(f), line(line_) {
       _KVSLOGGER_IMPL(func, line) << ">> Enter";
    }

    ~FtraceObject() {
       _KVSLOGGER_IMPL(func, line) << ">> Exit";
    }
};
#define FTRACE FtraceObject fobj(__FUNCTION__, __LINE__);
#else
#define FTRACE
#endif


#endif /* SRC_OS_KVSSTORE_KVSSTORE_DEBUG_H_ */
