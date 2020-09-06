/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(_WIN32)
#include <pthread.h>
typedef pthread_mutex_t MutexNativeObject;
#else
#include <windows.h>
typedef CRITICAL_SECTION MutexNativeObject;
#endif

class Mutex
{
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();
private:
    MutexNativeObject m;
    Mutex(const Mutex &);
    Mutex &operator=(const Mutex &);
};

class MutexHolder
{
public:
    explicit MutexHolder(Mutex &m) : m(m) { m.lock(); }
    ~MutexHolder() { m.unlock(); }
private:
    Mutex &m;
    MutexHolder(const MutexHolder &);
    MutexHolder &operator=(const MutexHolder &);
};

#if !defined(_WIN32)
inline Mutex::Mutex()
{
    pthread_mutex_init(&m, NULL);
}

inline Mutex::~Mutex()
{
    pthread_mutex_destroy(&m);
}

inline void Mutex::lock()
{
    pthread_mutex_lock(&m);
}

inline void Mutex::unlock()
{
    pthread_mutex_unlock(&m);
}
#else
inline Mutex::Mutex()
{
    InitializeCriticalSection(&m);
}

inline Mutex::~Mutex()
{
    DeleteCriticalSection(&m);
}

inline void Mutex::lock()
{
    EnterCriticalSection(&m);
}

inline void Mutex::unlock()
{
    LeaveCriticalSection(&m);
}
#endif
