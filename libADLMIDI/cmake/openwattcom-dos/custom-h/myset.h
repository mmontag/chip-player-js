/*
PtrList - A custom STD::Vector implementation. Workaround for OpenWatcom's crashing implementation

Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef MYSET_H
#define MYSET_H

#include <vector>

//WORKAROUND: use custom vector implementation to avoid lots of crashes!!!
#define _SET_INCLUDED
//#define set MySet

namespace std
{

template<class T, class Allocator = allocator< T >, class Implementation = vector<T, Allocator> >
class set : public Implementation
{
public:
    set() : Implementation() {}
    ~set() {}

    iterator find(const T& val)
    {
        for(iterator it = begin(); it != end(); it++)
        {
            if(val == *it)
                return it;
        }
        return end();
    }

    void insert (const T& val)
    {
        iterator f = find(val);
        if(f != end())
        {
            Implementation::insert(end(), val);
        }
    }

    void erase(const T& val)
    {
        iterator f = find(val);
        if(f != end())
        {
            Implementation::erase(f);
        }
    }

    //@{
    /**
     *  @brief  Finds the number of elements.
     *  @param  __x  Element to located.
     *  @return  Number of elements with specified key.
     *
     *  This function only makes sense for multisets; for set the result will
     *  either be 0 (not present) or 1 (present).
     */
    size_t
    count(const T& __x)
    {
        return find(__x) == Implementation::end() ? 0 : 1;
    }
};

}

#endif // MYSET_H
