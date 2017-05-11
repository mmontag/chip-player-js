#include "rar.hpp"

BitInput::BitInput()
{
  InBuf=new byte[MAX_SIZE];
}


BitInput::~BitInput()
{
  delete[] InBuf;
}


void BitInput::faddbits(uint Bits)
{
  addbits(Bits);
}


uint BitInput::fgetbits()
{
  return(getbits());
}
