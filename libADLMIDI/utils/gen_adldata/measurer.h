#ifndef MEASURER_H
#define MEASURER_H

#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <array>

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
    typedef std::array<int_fast32_t, 10> OperatorsKey;
    typedef std::map<OperatorsKey, DurationInfo> DurationInfoCacheX;

    MeasureThreaded();

    Semaphore           m_semaphore;
    std::mutex          m_durationInfo_mx;
    DurationInfoCacheX  m_durationInfo;
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
        BanksDump *bd;
        BanksDump::InstrumentEntry *bd_ins;
        std::thread       m_work;
        std::atomic_bool  m_works;

        void start();
        static void callback(void *myself);
    };

    std::vector<destData *>  m_threads;

    void printProgress();
    void printFinal();

    void run(BanksDump &bd, BanksDump::InstrumentEntry &e);
    void waitAll();
};

class OPLChipBase;
extern DurationInfo MeasureDurations(const BanksDump &db, const BanksDump::InstrumentEntry &ins, OPLChipBase *chip);

#endif // MEASURER_H
