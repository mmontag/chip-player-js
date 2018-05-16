/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLMIDI_PTR_HPP_THING
#define ADLMIDI_PTR_HPP_THING
/*
    Smart pointer for C heaps, created with malloc() call.
    FAQ: Why not std::shared_ptr? Because of Android NDK now doesn't supports it
*/
template<class PTR>
class AdlMIDI_CPtr
{
    PTR *m_p;
public:
    AdlMIDI_CPtr() : m_p(NULL) {}
    ~AdlMIDI_CPtr()
    {
        reset(NULL);
    }

    void reset(PTR *p = NULL)
    {
        if(p != m_p) {
            if(m_p)
                free(m_p);
            m_p = p;
        }
    }

    PTR *get()
    {
        return m_p;
    }
    PTR &operator*()
    {
        return *m_p;
    }
    PTR *operator->()
    {
        return m_p;
    }
private:
    AdlMIDI_CPtr(const AdlMIDI_CPtr &);
    AdlMIDI_CPtr &operator=(const AdlMIDI_CPtr &);
};

template<class PTR>
class AdlMIDI_NArrPtr
{
    PTR *m_p;
public:
    AdlMIDI_NArrPtr() : m_p(NULL) {}
    AdlMIDI_NArrPtr(PTR *value)
    {
        reset(value);
    }

    ~AdlMIDI_NArrPtr()
    {
        reset(NULL);
    }

    void reset(PTR *p = NULL)
    {
        if(p != m_p) {
            if(m_p)
                delete [] m_p;
            m_p = p;
        }
    }

    PTR *get()
    {
        return m_p;
    }
    PTR &operator*()
    {
        return *m_p;
    }
    PTR *operator->()
    {
        return m_p;
    }
    PTR operator[](size_t index)
    {
        return m_p[index];
    }
    const PTR operator[](size_t index) const
    {
        return m_p[index];
    }
    AdlMIDI_NArrPtr(AdlMIDI_NArrPtr &other)
    {
        m_p = other.m_p;
        other.m_p = NULL;
    }
    AdlMIDI_NArrPtr &operator=(AdlMIDI_NArrPtr &other)
    {
        m_p = other.m_p;
        other.m_p = NULL;
        return m_p;
    }
};

/*
  Generic deleters for smart pointers
 */
template <class T>
struct ADLMIDI_DefaultDelete
{
    void operator()(T *x) { delete x; }
};
template <class T>
struct ADLMIDI_DefaultArrayDelete
{
    void operator()(T *x) { delete[] x; }
};

/*
    Shared pointer with non-atomic counter
    FAQ: Why not std::shared_ptr? Because of Android NDK now doesn't supports it
*/
template< class VALUE, class DELETER = ADLMIDI_DefaultDelete<VALUE> >
class AdlMIDI_SPtr
{
    VALUE *m_p;
    size_t *m_counter;
public:
    AdlMIDI_SPtr() : m_p(NULL), m_counter(NULL) {}
    ~AdlMIDI_SPtr()
    {
        reset(NULL);
    }

    AdlMIDI_SPtr(const AdlMIDI_SPtr &other)
        : m_p(other.m_p), m_counter(other.m_counter)
    {
        if(m_counter)
            ++*m_counter;
    }

    AdlMIDI_SPtr &operator=(const AdlMIDI_SPtr &other)
    {
        if(this == &other)
            return *this;
        reset();
        m_p = other.m_p;
        m_counter = other.m_counter;
        if(m_counter)
            ++*m_counter;
        return *this;
    }

    void reset(VALUE *p = NULL)
    {
        if(p != m_p) {
            if(m_p && --*m_counter == 0) {
                DELETER del;
                del(m_p);
            }
            m_p = p;
            if(!p) {
                if(m_counter) {
                    delete m_counter;
                    m_counter = NULL;
                }
            }
            else
            {
                if(!m_counter)
                    m_counter = new size_t;
                *m_counter = 1;
            }
        }
    }

    VALUE *get() const
    {
        return m_p;
    }
    VALUE &operator*()
    {
        return *m_p;
    }
    const VALUE &operator*() const
    {
        return *m_p;
    }
    VALUE *operator->() const
    {
        return m_p;
    }
};

template<class VALUE>
class AdlMIDI_SPtrArray :
    public AdlMIDI_SPtr< VALUE, ADLMIDI_DefaultArrayDelete<VALUE> >
{
};

#endif //ADLMIDI_PTR_HPP_THING
