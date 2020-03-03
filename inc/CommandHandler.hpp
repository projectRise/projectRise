/*! CommandHandler.hpp
    Class for a more flexible command-like communication between two devices via serial
    \file       CommandHandler.hpp
    \authors    albrdev (albrdev@gmail.com)
    \date       2020-03-02
*/

#ifndef __COMMANDHANDLER_HPP__
#define __COMMANDHANDLER_HPP__

#include <string.h>
#include <stddef.h>
#include <Stream.h>

class CommandHandler;
typedef bool(*cmdhndlr_t)(CommandHandler*, const char*, const char*);

class CommandHandler
{
private:
    Stream& m_Source;

    char* m_Buffer;
    size_t m_Size;

    cmdhndlr_t m_Callback;

public:
    bool Receive(void);

    template<class T>
    inline size_t Send(const T& msg)
    {
        return m_Source.print(msg);
    }

    template<class T>
    inline size_t SendLine(const T& msg)
    {
        return m_Source.println(msg);
    }

    inline void Flush(void)
    {
        m_Source.flush();
    }

    CommandHandler(Stream& source, char* buffer, size_t size, cmdhndlr_t callback);
};

#endif // __COMMANDHANDLER_HPP__
