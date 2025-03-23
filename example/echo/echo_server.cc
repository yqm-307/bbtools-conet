#include <bbt/conet/conet.hpp>
#include "echoconnection.hpp"

class ServerConn;

std::map<int64_t, ServerConn*> m_conn_mgr;
std::mutex m_conn_mgr_mtx;

class ServerConn:
    public EchoConn<true>
{
public:
    ServerConn(std::shared_ptr<bbt::conet::detail::TIEventLoop> evloop,
             int fd,
             const bbt::conet::IPAddress& addr,
             int timeout
            ):
    EchoConn(evloop, fd, addr, timeout)
    {}

    virtual ~ServerConn() {}

    virtual void OnClose() override
    {
        EchoConn<true>::OnClose();
        std::unique_lock<std::mutex> _(m_conn_mgr_mtx);
        Assert(m_conn_mgr.erase(GetId()) > 0);
    }
private:
};

class EchoServer:
    public bbt::conet::TcpServer
{
public:

    EchoServer(std::shared_ptr<bbt::conet::detail::EventLoop> eventloop, const std::string& ip, short port):
        bbt::conet::TcpServer(eventloop, ip, port)
    {}

    virtual ~EchoServer() {}

    virtual void OnError(const bbt::network::Errcode& err) override
    {
        printf("[echo serv] %s\n", err.CWhat());
    }

    virtual void OnAccept(int socket, const bbt::conet::IPAddress& addr) override
    {
        auto conn = std::make_shared<ServerConn>(GetEventLoop(), socket, addr, 1000);
        conn->Run();
        printf("[echo serv] on accept! %d %s\n", socket, addr.GetIPPort().c_str());
        std::unique_lock<std::mutex> _(m_conn_mgr_mtx);
        auto [it, succ] = m_conn_mgr.insert(std::make_pair(conn->GetId(), conn.get()));
        Assert(succ);
    }
};

int main()
{
    auto eventloop = std::make_shared<bbt::conet::detail::EventLoop>(100, true);

    EchoServer serv{eventloop, "", 10101};

    serv.CoStart();

    while (1) {
        m_conn_mgr_mtx.lock();
        printf("[prefile] %d\n", m_conn_mgr.size());
        m_conn_mgr_mtx.unlock();
        sleep(1);
    }
}