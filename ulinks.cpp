#include "rar.hpp"



int ExtractLink(ComprDataIO &DataIO,Archive &Arc,char *DestName,uint &LinkCRC,bool Create)
{
#if defined(SAVE_LINKS) && defined(_UNIX)
  char FileName[NM];
  if (IsLink(Arc.NewLhd.FileAttr))
  {
    DataIO.UnpRead((byte *)FileName,Arc.NewLhd.PackSize);
    FileName[Arc.NewLhd.UnpSize]=0;
    if (Create)
    {
      CreatePath(DestName,NULL,true);
      if (symlink(FileName,DestName)==-1)
        if (errno==EEXIST)
          Log(Arc.FileName,St(MSymLinkExists),DestName);
        else
        {
          Log(Arc.FileName,St(MErrCreateLnk),DestName);
          ErrHandler.SetErrorCode(WARNING);
        }
    }
    LinkCRC=CRC(0xffffffff,FileName,Arc.NewLhd.UnpSize);
    return(1);
  }
#endif
  return(0);
}
