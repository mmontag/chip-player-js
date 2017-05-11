#ifndef _RAR_CMDDATA_
#define _RAR_CMDDATA_

#define DefaultStoreList "ace;arj;bz2;cab;gz;jpeg;jpg;lha;lzh;mp3;rar;zip;taz;tgz;z"

class CommandData:public RAROptions
{
  private:
    void ProcessSwitchesString(char *Str);
    void ProcessSwitch(char *Switch);
    void BadSwitch(char *Switch);
    uint GetExclAttr(char *Str);

    bool FileLists;
    bool NoMoreSwitches;
    bool TimeConverted;
  public:
    CommandData();
    ~CommandData();
    void Init();
    void Close();
    void ParseArg(char *Arg);
    void ParseDone();
    void ParseEnvVar();
    void ReadConfig(int argc,char *argv[]);
    bool IsConfigEnabled(int argc,char *argv[]);
    void OutTitle();
    void OutHelp();
    bool IsSwitch(int Ch);
    bool ExclCheck(char *CheckName,bool CheckFullPath);
    bool StoreCheck(char *CheckName);
    bool TimeCheck(uint FileDosTime);
    bool IsProcessFile(FileHeader &NewLhd,bool *ExactMatch=NULL);
    void ProcessCommand();
    void AddArcName(char *Name,wchar *NameW);
    bool GetArcName(char *Name,wchar *NameW,int MaxSize);
    bool CheckWinSize();

    int GetRecoverySize(char *Str,int DefSize);

    char Command[NM+16];

    char ArcName[NM];
    wchar ArcNameW[NM];

    StringList *FileArgs;
    StringList *ExclArgs;
    StringList *ArcNames;
    StringList *StoreArgs;
};

#endif
