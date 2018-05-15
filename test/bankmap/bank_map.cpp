#include <catch.hpp>
#include <random>
#include "adlmidi_bankmap.h"

typedef BasicBankMap<int> BankMap;
typedef BankMap::value_type ValuePair;
typedef BankMap::iterator Iterator;
typedef std::pair<Iterator, bool> InsertResult;

static std::mt19937 rng;

static size_t iterated_size(const BankMap &map)
{
    size_t size = 0;
    auto it = map.begin(), end = map.end();
    while(it != end) {
        ++it;
        ++size;
    }
    return size;
}

static bool consistent_size(const BankMap &map)
{
    return map.size() == iterated_size(map);
}

TEST_CASE("[BankMap] Construction")
{
    BankMap map;

    REQUIRE(map.capacity() == 0);
    REQUIRE(map.size() == 0);
    REQUIRE(iterated_size(map) == 0);
}

TEST_CASE("[BankMap] Insert, erase and find")
{
    BankMap map;

    for(unsigned i = 0; i < 10; ++i) {
        const uint16_t key = rng();
        const int value = rng();

        uint16_t another_key = rng();
        while (another_key == key)
            another_key = rng();

        InsertResult ir = map.insert(ValuePair{key, value});
        REQUIRE(ir.second);
        REQUIRE(map.size() == 1);
        REQUIRE(iterated_size(map) == 1);

        Iterator it = map.find(key);
        REQUIRE(it != map.end());
        REQUIRE(it->first == key);
        REQUIRE(it->second == value);
        REQUIRE(map.find(another_key) == map.end());

        map.erase(it);
        REQUIRE(map.find(key) == map.end());
        REQUIRE(map.size() == 0);
        REQUIRE(iterated_size(map) == 0);
    }
}

TEST_CASE("[BankMap] Insert without expanding")
{
    BankMap map;

    InsertResult ir;
    ir = map.insert(ValuePair{0, 0}, BankMap::do_not_expand_t());
    REQUIRE(ir.first == map.end());
    REQUIRE(!ir.second);

    map.reserve(1);
    REQUIRE(map.capacity() > 1);

    while(map.size() < map.capacity()) {
        const uint16_t key = rng();
        const int value = rng();
        if(map.find(key) != map.end())
            continue;
        ir = map.insert(ValuePair{key, value}, BankMap::do_not_expand_t());
        REQUIRE(ir.first != map.end());
        REQUIRE(ir.first->first == key);
        REQUIRE(ir.first->second == value);
        REQUIRE(ir.second);
        REQUIRE(consistent_size(map));
    }

    ir = map.insert(ValuePair{0, 0}, BankMap::do_not_expand_t());
    REQUIRE(ir.first == map.end());
    REQUIRE(!ir.second);
    REQUIRE(consistent_size(map));

    map.clear();
    REQUIRE(map.size() == 0);
    REQUIRE(consistent_size(map));
}
