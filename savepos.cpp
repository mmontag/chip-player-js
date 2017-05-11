#include "rar.hpp"

SaveFilePos::SaveFilePos(File &SaveFile)
{
  SaveFilePos::SaveFile=&SaveFile;
  SavePos=SaveFile.Tell();
}


SaveFilePos::~SaveFilePos()
{
  SaveFile->Seek(SavePos,SEEK_SET);
}
