/*
 * INI Processor - a small library which allows you parsing INI-files
 *
 * Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INIPROCESSING_H
#define INIPROCESSING_H

#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#ifdef INI_PROCESSING_ALLOW_QT_TYPES
#include <QString>
#include <QList>
#include <QVector>
#endif

#include "ini_processing_variant.h"

/**
 * @brief INI Processor - an utility which providing fast and flexible INI file parsing API
 */
class IniProcessing
{
public:
    /**
     * @brief Available error codes
     */
    enum ErrCode
    {
        //! Everything is fine
        ERR_OK = 0,
        //! File not found or memory pointer is null
        ERR_NOFILE,
        //! Invalid section declaration syntax
        ERR_SECTION_SYNTAX,
        //! Invalid key declaration syntax
        ERR_KEY_SYNTAX,
        //! No available memory
        ERR_NO_MEMORY
    };

private:
    struct      params
    {
        std::string filePath;
        bool        opened;
        int         lineWithError;
        ErrCode     errorCode;
        bool        modified;
        typedef     std::unordered_map<std::string, std::string> IniKeys;
        typedef     std::unordered_map<std::string, IniKeys> IniSections;
        IniSections iniData;
        IniKeys    *currentGroup;
        std::string currentGroupName;
    } m_params;

    template<class TList, typename T>
    friend void readNumArrHelper(IniProcessing* self, const char *key, TList &dest, const TList &defVal);

    bool parseHelper(char *data, size_t size);

    bool parseFile(const char *filename);
    bool parseMemory(char *mem, size_t size);

    inline params::IniKeys::iterator readHelper(const char *key, bool &ok)
    {
        if(!m_params.opened)
            return params::IniKeys::iterator();

        if(!m_params.currentGroup)
            return params::IniKeys::iterator();

        #ifndef CASE_SENSITIVE_KEYS
        std::string key1(key);
        for(char *iter = &key1[0]; *iter != '\0'; ++iter)
            *iter = (char)tolower(*iter);
        #endif

        params::IniKeys::iterator e = m_params.currentGroup->find(key1);

        if(e != m_params.currentGroup->end())
            ok = true;

        return e;
    }

    void writeIniParam(const char *key, const std::string &value);

public:
    IniProcessing();
    IniProcessing(const char *iniFileName, int dummy = 0);
    IniProcessing(const std::string &iniFileName, int dummy = 0);
#ifdef INI_PROCESSING_ALLOW_QT_TYPES
    IniProcessing(const QString &iniFileName, int dummy = 0);
#endif
    IniProcessing(char *memory, size_t size);
    IniProcessing(const IniProcessing &ip);

    /**
     * @brief Open INI-file from disk and parse it
     * @param iniFileName full path to INI-file to parse
     * @return true if INI file has been passed, false if any error happen
     */
    bool open(const std::string &iniFileName);

    /**
     * @brief Open raw INI-data from memory and parse it
     * @param memory pointer to memory block
     * @param size size of memory block to process
     * @return
     */
    bool openMem(char *memory, size_t size);

    /**
     * @brief Clear all internal buffers and close the file
     */
    void close();

    /**
     * @brief Returns last happen error code
     * @return Error code
     */
    ErrCode lastError();
    /**
     * @brief Line number which contains error
     * @return line number wit herror
     */
    int  lineWithError();

    /**
     * @brief State of INI Processor
     * @return true if any file is opened
     */
    bool isOpened();

    /**
     * @brief Select a section to process
     * @param groupName name of section to process
     * @return true if group exists and opened
     */
    bool beginGroup(const std::string &groupName);

    /**
     * @brief Is this INI file contains section of specific name
     * @param groupName name of section
     * @return true if section is exists
     */
    bool contains(const std::string &groupName);

    /**
     * @brief Currently opened file name
     * @return path to currently opened file
     */
    std::string fileName();

    /**
     * @brief Currently processing section
     * @return name of current section
     */
    std::string group();

    /**
     * @brief Get list of available groups
     * @return Array of strings
     */
    std::vector<std::string> childGroups();

    /**
     * @brief Is current section contains specific key name
     * @param keyName name of key
     * @return true if key is presented in this section
     */
    bool hasKey(const std::string &keyName);

    /**
     * @brief Get list of available keys in current groul
     * @return Array of strings
     */
    std::vector<std::string> allKeys();

    /**
     * @brief Release current section to choice another for process
     */
    void endGroup();

    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, bool &dest, bool defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, unsigned char &dest, unsigned char defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, char &dest, char defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, unsigned short &dest, unsigned short defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, short &dest, short defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, unsigned int &dest, unsigned int defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, int &dest, int defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, unsigned long &dest, unsigned long defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, long &dest, long defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, unsigned long long &dest, unsigned long long defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, long long &dest, long long defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, float &dest, float defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, double &dest, double defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, long double &dest, long double defVal);
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::string &dest, const std::string &defVal);

    #ifdef INI_PROCESSING_ALLOW_QT_TYPES
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QString &dest, const QString &defVal);
    #endif

    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<unsigned short> &dest, const std::vector<unsigned short> &defVal = std::vector<unsigned short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<short> &dest, const std::vector<short> &defVal = std::vector<short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<unsigned int> &dest, const std::vector<unsigned int> &defVal = std::vector<unsigned int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<int> &dest, const std::vector<int> &defVal = std::vector<int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<unsigned long> &dest, const std::vector<unsigned long> &defVal = std::vector<unsigned long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<long> &dest, const std::vector<long> &defVal = std::vector<long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<unsigned long long> &dest, const std::vector<unsigned long long> &defVal = std::vector<unsigned long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<long long> &dest, const std::vector<long long> &defVal = std::vector<long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<float> &dest, const std::vector<float> &defVal = std::vector<float>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<double> &dest, const std::vector<double> &defVal = std::vector<double>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, std::vector<long double> &dest, const std::vector<long double> &defVal = std::vector<long double>());

    #ifdef INI_PROCESSING_ALLOW_QT_TYPES
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<short> &dest, const QList<short> &defVal = QList<short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<unsigned short> &dest, const QList<unsigned short> &defVal = QList<unsigned short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<int> &dest, const QList<int> &defVal = QList<int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<unsigned int> &dest, const QList<unsigned int> &defVal = QList<unsigned int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<long> &dest, const QList<long> &defVal = QList<long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<unsigned long> &dest, const QList<unsigned long> &defVal = QList<unsigned long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<long long> &dest, const QList<long long> &defVal = QList<long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<unsigned long long> &dest, const QList<unsigned long long> &defVal = QList<unsigned long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<float> &dest, const QList<float> &defVal = QList<float>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<double> &dest, const QList<double> &defVal = QList<double>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QList<long double> &dest, const QList<long double> &defVal = QList<long double>());

    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<short> &dest, const QVector<short> &defVal = QVector<short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<unsigned short> &dest, const QVector<unsigned short> &defVal = QVector<unsigned short>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<int> &dest, const QVector<int> &defVal = QVector<int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<unsigned int> &dest, const QVector<unsigned int> &defVal = QVector<unsigned int>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<long> &dest, const QVector<long> &defVal = QVector<long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<unsigned long> &dest, const QVector<unsigned long> &defVal = QVector<unsigned long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<long long> &dest, const QVector<long long> &defVal = QVector<long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<unsigned long long> &dest, const QVector<unsigned long long> &defVal = QVector<unsigned long long>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<float> &dest, const QVector<float> &defVal = QVector<float>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<double> &dest, const QVector<double> &defVal = QVector<double>());
    /**
     * @brief Retreive value by specific key and pass it via reference
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     */
    void read(const char *key, QVector<long double> &dest, const QVector<long double> &defVal = QVector<long double>());
    #endif

    //! Hash-table for the fast string to enum conversion
    typedef std::unordered_map<std::string, int> StrEnumMap;


    template<typename T>
    /**
     * @brief Retreive value by string-based enum key
     * @param [_IN] key name of key with value to retrieved
     * @param [_OUT] dest Reference to destination variable to store retrieved value
     * @param [_IN] defVal Default value for case of non-existing key
     * @param [_IN] enumMap
     */
    void readEnum(const char *key, T &dest, T defVal, IniProcessing::StrEnumMap enumMap)
    {
        bool ok = false;
        params::IniKeys::iterator e = readHelper(key, ok);

        if(!ok)
        {
            dest = defVal;
            return;
        }

        StrEnumMap::iterator em = enumMap.find(e->second);

        if(em == enumMap.end())
        {
            dest = defVal;
            return;
        }

        dest = static_cast<T>(em->second);
    }

    /**
     * @brief QSettings-compatible way to retreive value
     * @param key key with value to retreive
     * @param defVal default value key
     * @return variant which contains a value
     */
    IniProcessingVariant value(const char *key, const IniProcessingVariant &defVal = IniProcessingVariant());

    void setValue(const char *key, unsigned short value);
    void setValue(const char *key, short value);
    void setValue(const char *key, unsigned int value);
    void setValue(const char *key, int value);
    void setValue(const char *key, unsigned long value);
    void setValue(const char *key, long value);
    void setValue(const char *key, unsigned long long value);
    void setValue(const char *key, long long value);
    void setValue(const char *key, float value);
    void setValue(const char *key, double value);
    void setValue(const char *key, long double value);

    template <typename T>
    static inline std::string to_string_with_precision(const T a_value)
    {
        char buf[35];
        memset(buf, 0, 35);
        snprintf(buf, 34, "%.15g", static_cast<double>(a_value));
        return buf;
    }

    template<class TList>
    static inline std::string fromVector(const TList &value)
    {
        typedef typename TList::value_type T;
        std::string out;
        for(const T &f: value)
        {
            if(!out.empty())
                out.push_back(',');
            if(std::is_same<T, float>::value ||
               std::is_same<T, double>::value ||
               std::is_same<T, long double>::value)
                out.append(to_string_with_precision(f));
            else
                out.append(std::to_string(f));
        }
        return out;
    }

    template<typename T>
    void setValue(const char *key, const std::vector<T> &value)
    {
        static_assert(std::is_arithmetic<T>::value, "Not arithmetic (integral or floating point required!)");
        writeIniParam(key, fromVector(value));
    }

    void setValue(const char *key, const char *value);
    void setValue(const char *key, const std::string &value);

    #ifdef INI_PROCESSING_ALLOW_QT_TYPES
    void setValue(const char *key, const QString &value);

    template<typename T>
    void setValue(const char *key, const QList<T> &value)
    {
        static_assert(std::is_arithmetic<T>::value, "Not arithmetic (integral or floating point required!)");
        writeIniParam(key, fromVector(value));
    }

    template<typename T>
    void setValue(const char *key, const QVector<T> &value)
    {
        static_assert(std::is_arithmetic<T>::value, "Not arithmetic (integral or floating point required!)");
        writeIniParam(key, fromVector(value));
    }
    #endif

    /**
     * @brief Write INI file by the recently given file path
     * @return true if INI file was successfully written
     */
    bool writeIniFile();
};

#endif // INIPROCESSING_H
