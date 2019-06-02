#ifndef MEASURER_H
#define MEASURER_H

#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "progs_cache.h"

struct DurationInfo
{
    uint64_t    peak_amplitude_time;
    double      peak_amplitude_value;
    double      quarter_amplitude_time;
    double      begin_amplitude;
    double      interval;
    double      keyoff_out_time;
    int64_t     ms_sound_kon;
    int64_t     ms_sound_koff;
    bool        nosound;
    uint8_t     padding[7];
};

class Semaphore
{
public:
    Semaphore(int count_ = 0)
        : m_count(count_) {}

    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        m_count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        while(m_count == 0)
        {
            cv.wait(lock);
        }
        m_count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic_int m_count;
};

struct MeasureThreaded
{
    typedef std::map<ins, DurationInfo> DurationInfoCache;

    MeasureThreaded() :
        m_semaphore(int(std::thread::hardware_concurrency()) * 2),
        m_done(0),
        m_cache_matches(0)
    {}

    Semaphore           m_semaphore;
    std::mutex          m_durationInfo_mx;
    DurationInfoCache   m_durationInfo;
    std::atomic_bool    m_delete_tail;
    size_t              m_total = 0;
    std::atomic<size_t> m_done;
    std::atomic<size_t> m_cache_matches;

    void LoadCache(const char *fileName);
    void SaveCache(const char *fileName);

    struct destData
    {
        destData()
        {
            m_works = true;
        }
        ~destData()
        {
            m_work.join();
        }
        MeasureThreaded  *myself;
        std::map<ins, std::pair<size_t, std::set<std::string> > >::const_iterator i;
        std::thread       m_work;
        std::atomic_bool  m_works;

        void start();
        static void callback(void *myself);
    };

    std::vector<destData *>  m_threads;

    void printProgress();
    void printFinal();
    void run(InstrumentsData::const_iterator i);
    void waitAll();
};

class OPLChipBase;
extern DurationInfo MeasureDurations(const ins &in, OPLChipBase *chip);

#endif // MEASURER_H
