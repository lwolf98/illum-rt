// mostly taken over from niho's codebase
#include "timer.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

void timer::start(const std::string& name) { starts[name] = std::chrono::steady_clock::now(); }

void timer::stop(const std::string& name) {
    times[name] += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - starts[name]) .count();
    counts[name]++;
}

void timer::clear() {
    starts.clear();
    times.clear();
    counts.clear();
}

void timer::merge(const timer& t) {
    for (auto &curr : t.times)
        times[curr.first] += curr.second;
    for (auto &curr : t.counts)
        counts[curr.first] += curr.second;
}

uint64_t timer::ns_total() const {
    uint64_t total = 0;
    for (auto &t : times)
        total += times.at(t.first);
    return total;
}

uint64_t timer::count_total() const {
    uint64_t total = 0;
    for (auto &t : counts)
        total += counts.at(t.first);
    return total;
}

std::string timer::format(const std::string& name) const {
    uint64_t n = count(name);
    if (n > 1)
        return format(ms(name), ms_total(), n);
    else
        return format(ms(name), ms_total());
}

std::string timer::format(double ms_elapsed, double ms_total) {
    std::stringstream time;
    time.precision(1);
    uint64_t elapsed_seconds = uint64_t(ms_elapsed / 1000) % 60;
    uint64_t elapsed_minutes = uint64_t(ms_elapsed / 60000);
    time << std::fixed << std::right
	     << std::setw(10) << std::to_string(elapsed_minutes) << "m, "
	     << std::setw(2) << std::to_string(elapsed_seconds) << "s, "
	     << std::setw(3) << std::to_string(uint64_t(ms_elapsed) % 1000) << "ms";
    time << std::fixed << std::right << std::setw(5) << " [" << std::setw(5) << ms_elapsed / ms_total * 100 << "%]";
    return time.str();
}

std::string timer::format(double ms_elapsed, double ms_total, uint64_t count) {
    std::stringstream time;
    time.precision(1);
    uint64_t elapsed_seconds = uint64_t(ms_elapsed / 1000) % 60;
    uint64_t elapsed_minutes = uint64_t(ms_elapsed / 60000);
    time << std::fixed << std::right
	     << std::setw(10) << std::to_string(elapsed_minutes) << "m, "
	     << std::setw(2) << std::to_string(elapsed_seconds) << "s, "
	     << std::setw(3) << std::to_string(uint64_t(ms_elapsed) % 1000) << "ms";
    time << std::fixed << std::right << std::setw(5) << " [" << std::setw(5) << ms_elapsed / ms_total * 100 << "%]";
	time << std::fixed << std::right << std::setw(10) << "(" << ms_elapsed / count * 1000000 << "ns * " << count/1000000.0 << "M calls)";
    return time.str();
}

void timer::print(const std::string& timer_name) {
    if (times.empty())
        return;
    std::vector<std::pair<std::string, uint64_t>> sorted_times(times.begin(), times.end());
    std::sort(sorted_times.begin(), sorted_times.end(),
			  [](const std::pair<std::string, uint64_t> &a, const std::pair<std::string, uint64_t> &b) {
			  	return b.second < a.second;
			  });
    std::cout << (timer_name.size() ? timer_name : "Times taken") << ":" << std::endl;
    for (auto &t : sorted_times) {
        // output timing
        std::cout << std::left << std::setw(25) << t.first;
        std::cout << format(t.first) << std::endl;
    }
    // output total
    std::cout << std::left << std::setw(25) << "Total:";
    std::cout << std::right << std::setw(25) << format(ms_total(), ms_total()) << std::endl;
    std::cout << std::endl;
}

omp_timer stats_timer;
