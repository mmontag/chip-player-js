#include "opl_chip_base.h"

OPLChipBase::OPLChipBase() :
    m_rate(44100)
{}

OPLChipBase::OPLChipBase(const OPLChipBase &c):
    m_rate(c.m_rate)
{}

OPLChipBase::~OPLChipBase()
{}

void OPLChipBase::setRate(uint32_t rate)
{
    m_rate = rate;
}

void OPLChipBase::reset(uint32_t rate)
{
    setRate(rate);
}
