#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "dma_utils.c"

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <limits>
#include <cassert>
#include <algorithm>
#include <chrono> // for std::chrono functions


// helper functions


struct LogStream
{
    static std::mutex _ls_mux;

    LogStream() {
    }
    ~LogStream() {
    }
    template<typename T>
    friend LogStream& operator<<(LogStream& out, const T & value) {
        std::unique_lock<std::mutex> lk(_ls_mux);
        std::cout<<value;
        return out;
    }
};

inline LogStream&
log(bool ts = false) {
    static LogStream l;
    if (ts) {
        auto t = std::time(nullptr);
        std::string ts = std::asctime(std::gmtime(&t));
        ts.pop_back();
        l << ts <<" ";
    }
    return l;
}



class Timer
{
private:
 // Type aliases to make accessing nested type easier
 using clock_type = std::chrono::steady_clock;
 using second_type = std::chrono::duration<double, std::ratio<1> >;

 std::chrono::time_point<clock_type> m_beg { clock_type::now() };

public:
 void reset()
 {
  m_beg = clock_type::now();
 }

 double elapsed() const
 {
  return std::chrono::duration_cast<second_type>(clock_type::now() - m_beg).count();
 }
};
