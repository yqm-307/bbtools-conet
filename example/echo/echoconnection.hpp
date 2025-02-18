#pragma once
#include <bbt/conet/conet.hpp>
#include <bbt/base/clock/Clock.hpp>

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
            Send(bbt::core::Buffer{byte, len});
        if (m_last_print_recv_time < bbt::clock::now<bbt::clock::seconds>().time_since_epoch().count())
            return;
        printf("[echoconn][onrecv] total recv %ld bytes\n", len);
        m_last_print_recv_time = m_last_print_recv_time + 2;
    }

    virtual void OnTimeout() override
    {
        printf("[echoconn][ontimeout] timeout!\n");
    }

    virtual void OnSend(size_t len) override
    {
        m_send += len;
        if (m_last_print_send_time < bbt::clock::now<bbt::clock::seconds>().time_since_epoch().count())
            return;
        printf("[echoconn][onsend][%ld] total send %d bytes\n", GetId(), m_send);
        m_last_print_send_time = m_last_print_send_time + 2;
    }

    virtual void OnClose() override
    {
        printf("[echoconn][onclose][%ld] total recv=%ld\n", GetId(), m_recv);
    }

    virtual void OnError(const bbt::network::Errcode& err) override
    {
        printf("[echoconn][onerror] %s\n", err.CWhat());
        Close();
    }
private:
    size_t m_recv{0};
    size_t m_send{0};
    int    m_last_print_recv_time{0};
    int    m_last_print_send_time{0};
};