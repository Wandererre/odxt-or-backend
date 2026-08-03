#include "rawdatautil.h"

int DB_StrToHex2(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    temp[0] = text[0];
    temp[1] = text[1];
    hexarr[0] = ::strtoul(temp,nullptr,16) & 0xFF;
    return 0;
}

int DB_StrToHex(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<2; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex8(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<4; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex8(unsigned char *hexarr,const char *text)
{
    char temp[2];
    for (int j=0; j<4; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex12(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<12; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex16(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<16; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex32(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<32; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex49(unsigned char *hexarr,unsigned char *text)
{
    char temp[2];
    for (int j=0; j<49; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

int DB_StrToHex48(unsigned char *hexarr,const char *text)
{
    char temp[2];
    for (int j=0; j<48; j++)
    {
        temp[0] = text[2*j];
        temp[1] = text[2*j+1];
        hexarr[j] = ::strtoul(temp,nullptr,16) & 0xFF;
    }
    return 0;
}

std::string DB_HexToStr(unsigned char *hexarr)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 16; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string DB_HexToStr_N(unsigned char *hexarr, unsigned int n)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < n; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string DB_HexToStr32(unsigned char *hexarr)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 32; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string DB_HexToStr2(unsigned char *hexarr)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 2; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string DB_HexToStr8(unsigned char *hexarr)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 4; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}

std::string DB_HexToStr12(unsigned char *hexarr)
{
    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 12; ++i)
        ss << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hexarr[i]);
    return ss.str();
}