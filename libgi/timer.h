// mostly taken over from niho's codebase
#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <omp.h>
#include <mutex>

/*  \brief Wall-timer.
 *
 *  Note: We often use this timer in very small sections and the results might thus be skewed to some degree. Take the
 *  resulting measurements with a grain of salt.
 *
 *  To time a block, use \ref time_thid_block
 *
 */
struct timer {
	typedef std::chrono::time_point<std::chrono::steady_clock> timepoint;
    virtual ~timer() {}

    virtual void start(const std::string &name);
    virtual void stop(const std::string &name);

    void clear();
    void merge(const timer& t);

    inline uint64_t ns(const std::string& name) const { return times.at(name); }
    inline double   ms(const std::string &name) const { return ns(name) / 1000000.0; }
    inline double   s(const std::string &name) const { return ms(name) / 1000.0; }
    inline double   m(const std::string &name) const { return s(name) / 60.0; };
    inline double   h(const std::string &name) const { return m(name) / 60.0; };

    uint64_t      ns_total() const;
    inline double ms_total() const { return ns_total() / 1000000.0; }
    inline double s_total() const { return ms_total() / 1000.0; }
    inline double m_total() const { return s_total() / 60.0; }
    inline double h_total() const { return m_total() / 60.0; }

    inline uint64_t count(const std::string& name) const { return counts.at(name); }
    uint64_t count_total() const;

    std::string format(const std::string& name) const;
    static std::string format(double ms_elapsed, double ms_total);
    static std::string format(double ms_elapsed, double ms_total, uint64_t count);

    void print(const std::string& timer_name = "");

    std::map<std::string, timepoint> starts;
    std::map<std::string, uint64_t> times;
    std::map<std::string, uint64_t> counts;
};

struct omp_timer {
    omp_timer() : timers(omp_get_max_threads()) {}
    virtual ~omp_timer() {}

    virtual void start(const std::string &name) { timers[omp_get_thread_num()].start(name); }

    virtual void stop(const std::string &name) { timers[omp_get_thread_num()].stop(name); }

    void clear() {
        for (auto &t : timers)
            t.clear();
    }

    void print(const std::string &timer_name = "") {
        timer total;
        for (auto &t : timers)
            total.merge(t);
        total.print(timer_name);
    }

    std::vector<timer> timers;
};

extern omp_timer stats_timer;

struct raii_timer {
    raii_timer(const std::string &name) : name(name) { stats_timer.start(name); }
    ~raii_timer() { stats_timer.stop(name); }

    std::string name; ///< timed section's name
};

#define WITH_STATS

#ifdef WITH_STATS
#define time_this_block(name) raii_timer raii_timer__##name(#name)
#else
#define time_this_block(name)
#endif
