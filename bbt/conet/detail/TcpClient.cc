#include <bbt/conet/detail/TcpClient.hpp>

namespace bbt::network::conet::detail
{

void TcpClient::CoConnect(const std::string& ip, short port)
{
    bbtco [=](){
        _ConnectCo(ip, port);
    };
}

TcpClient::ConnectResult TcpClient::Connect(const std::string& ip, short port)
{
    return _ConnectCo(ip, port);
}

TcpClient::ConnectResult TcpClient::_ConnectCo(const std::string& ip, short port)
{
    /**
     * 调用此函数表现分为两种情况
     * 1、在协程线程中执行
     * 2、在非协程线程中执行
     * 
     * 这里处理connect时是视为在非协程线程中执行，因为协程执行是兼容
     * 非协程执行的。
     * 
     */
    IPAddress addr{ip, port};
    int socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0)
        return {Errcode{"tcpclient socket() error! errno=" + std::to_string(errno), ErrType::ERRTYPE_ERROR}, nullptr};
    
    int err = ::connect(socket, addr.getsockaddr(), addr.getsocklen());
    if (err != 0) {
        ::close(socket);
        if (errno == EINTR || errno == EINPROGRESS)
            return {Errcode{"try again", ErrType::ERRTYPE_CONNECT_TRY_AGAIN}, nullptr};

        if (errno == ECONNREFUSED)
            return {Errcode{"connect refused", ErrType::ERRTYPE_CONNECT_CONNREFUSED}, nullptr};
        
        return {std::nullopt, nullptr};
    }

    return OnConnect(socket, addr);
}

std::shared_ptr<TIEventLoop> TcpClient::GetEventLoop()
{
    return m_event_loop.lock();
}


}
