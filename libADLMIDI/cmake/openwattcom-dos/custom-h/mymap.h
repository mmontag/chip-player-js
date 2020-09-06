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

#ifndef MYMAP_H
#define MYMAP_H

#include <vector>
#include <utility>
#include <cstdio>

//WORKAROUND: use custom std::map implementation to avoid lots of crashes!!!
#define _MAP_INCLUDED

namespace std
{

template<class Key,
         class Type,
         class Allocator = allocator< pair<Key, Type > >,
         class Implementation = vector< pair<Key, Type >, Allocator> >
class map : public Implementation
{
public:
    map() : Implementation() {}
    ~map() {}

    iterator find(const Key& val)
    {
        std::printf("std::map find\r\n");
        std::fflush(stdout);

        for(iterator it = begin(); it != end(); it++)
        {
            if(val == it->first)
                return it;
        }
        return end();
    }

    map& operator=( map const & x )
    {
        std::printf("std::map operator=\r\n");
        std::fflush(stdout);

        Implementation::operator=( x ); return( *this );
    }

    Type& operator[](const Key &key)
    {
        std::printf("std::map operator[]\r\n");
        std::fflush(stdout);

        for(iterator it = begin(); it != end(); it++)
        {
            if(key == it->first)
                return it->second;
        }
        return Implementation::insert(end(), pair<Key, Type>(key, Type()))->second;
    }

    pair<iterator, bool> insert(const pair<Key, Type> &val)
    {
        std::printf("std::map insert\r\n");
        std::fflush(stdout);

        bool found = false;
        iterator f = find(val.first);
        if(f != end())
        {
            f->second = val.second;
            found = true;
        }
        else
        {
            f = Implementation::insert(end(), val);
        }
        return make_pair<iterator, bool>(f, found);
    }

    void erase(const iterator& val)
    {
        std::printf("std::map erase iterator\r\n");
        std::fflush(stdout);

        if(val != end())
            Implementation::erase(val);
    }

    void erase(const Key& val)
    {
        std::printf("std::map erase key\r\n");
        std::fflush(stdout);

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
    count(const Key& __x)
    {
        return find(__x) == Implementation::end() ? 0 : 1;
    }
};

}

#endif // MYMAP_H
