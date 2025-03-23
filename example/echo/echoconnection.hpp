#pragma once
#include <bbt/conet/conet.hpp>
#include <bbt/core/clock/Clock.hpp>

using namespace bbt::core;

template<bool CanEcho>
class EchoConn:
    public bbt::conet::detail::Connection
{
public:
    EchoConn(std::shared_ptr<bbt::conet::detail::TIEventLoop> evloop,
             int fd,
             const bbt::conet::IPAddress& addr,
             int timeout
            ):
            bbt::conet::detail::Connection(evloop, fd, addr, timeout)
    {}

    virtual ~EchoConn() {}

    virtual void OnRecv(const char* byte, size_t len) override
    {
        m_recv += len;
        if (CanEcho)
            if (auto err = Send(bbt::core::Buffer{byte, len}); err.has_value())
                OnError(err.value());
        if (m_last_print_recv_time < bbt::core::clock::now<bbt::core::clock::seconds>().time_since_epoch().count())
            return;
        printf("%s[echoconn][onrecv] total recv %ld bytes\n", clock::getnow_str().c_str(), len);
        m_last_print_recv_time = m_last_print_recv_time + 2;
    }

    virtual void OnTimeout() override
    {
        printf("%s[echoconn][ontimeout][%ld] timeout!\n", clock::getnow_str().c_str(), GetId());
    }

    virtual void OnSend(size_t len) override
    {
        m_send += len;
        if (m_last_print_send_time < bbt::core::clock::now<bbt::core::clock::seconds>().time_since_epoch().count())
            return;
        printf("%s[echoconn][onsend][%ld] total send %d bytes\n", clock::getnow_str().c_str(), GetId(), m_send);
        m_last_print_send_time = m_last_print_send_time + 2;
    }

    virtual void OnClose() override
    {
        printf("%s[echoconn][onclose][%ld] total recv=%ld total send=%ld\n", clock::getnow_str().c_str(), GetId(), m_recv, m_send);
    }

    virtual void OnError(const bbt::network::Errcode& err) override
    {
        printf("%s[echoconn][onerror] %s\n", clock::getnow_str().c_str(), err.CWhat());
        Close();
    }
private:
    size_t m_recv{0};
    size_t m_send{0};
    int    m_last_print_recv_time{0};
    int    m_last_print_send_time{0};
};