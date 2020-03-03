#include "CommandHandler.hpp"

bool CommandHandler::Receive(void)
{
    size_t len = m_Source.readBytesUntil('\n', m_Buffer, m_Size);
    if(len == 0U)
    {
        return m_Callback(this, nullptr, nullptr);
    }
    else if(m_Buffer[len - 1] != '\r')
    {
        return m_Callback(this, nullptr, m_Buffer);
    }

    m_Buffer[len - 1U] = '\0';

    char* argPos = strchr(m_Buffer, ' ');
    if(argPos != nullptr)
    {
        *argPos = '\0';
        argPos++;
    }

    return m_Callback(this, m_Buffer, argPos);
}

CommandHandler::CommandHandler(Stream& source, char* buffer, size_t size, cmdhndlr_t callback) : m_Source(source), m_Buffer(buffer), m_Size(size), m_Callback(callback) { }
