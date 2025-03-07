#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>
#include <bbt/conet/conet.hpp>
#include <bbt/core/net/SocketUtil.hpp>

using namespace bbt::network::conet;

std::shared_ptr<detail::EventLoop> eventloop;
char msg[] = "hello world";

class TestConn:
    public detail::Connection
{
public:
    TestConn(std::shared_ptr<detail::EventLoop> evloop, int fd, const IPAddress& addr, int timeout):
        detail::Connection(evloop, fd, addr, timeout)
    {}
protected:
    virtual void OnRecv(const char* byte, size_t len) override
    {
        printf("[recv] %s\n", byte);
    }

    virtual void OnTimeout() override {}
    virtual void OnSend(size_t len) override 
    {
        printf("[send] succ!\n");
    }
    virtual void OnClose() override {}
    virtual void OnError(const bbt::network::Errcode& err) override {}
};

BOOST_AUTO_TEST_SUITE(ConnectionTest)

BOOST_AUTO_TEST_CASE(t_begin)
{
    eventloop = std::make_shared<detail::EventLoop>(1000, true);
}

BOOST_AUTO_TEST_CASE(t_connection_send_recv)
{

    bbt::core::thread::CountDownLatch l{2};

    bbtco_desc("server") [&l]()
    {
        BOOST_TEST_MESSAGE("[server] server co=" << bbt::coroutine::GetLocalCoroutineId());
        int fd = bbt::core::net::Util::CreateListen("", 10001, true);
        BOOST_ASSERT(fd >= 0);
        BOOST_TEST_MESSAGE("[server] create succ listen fd=" << fd);
        sockaddr_in cli_addr;
        char *buf = new char[1024];
        memset(buf, '\0', 1024);
        socklen_t len = sizeof(cli_addr);

        int new_fd = ::accept(fd, (sockaddr *)(&cli_addr), &len);
        BOOST_ASSERT(new_fd >= 0);
        BOOST_TEST_MESSAGE("[server] accept succ new_fd=" << new_fd);
        int read_len = ::read(new_fd, buf, 1024);
        BOOST_CHECK_GT(read_len, 0);
        BOOST_ASSERT(std::string{msg} == std::string{buf});
        BOOST_TEST_MESSAGE("[server] recv" << std::string{buf});
        ::close(fd);
        ::close(new_fd);
        l.Down();
        BOOST_TEST_MESSAGE("[server] exit!");
    };

    bbtco_desc("client") [&l]()
    {
        ::sleep(1);
        BOOST_TEST_MESSAGE("[client] client co=" << bbt::coroutine::GetLocalCoroutineId());
        IPAddress addr{"127.0.0.1", 10001};
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        BOOST_ASSERT(fd >= 0);
        BOOST_TEST_MESSAGE("[client] create socket succ!");
        int ret = ::connect(fd, addr.getsockaddr(), addr.getsocklen());
        BOOST_CHECK_MESSAGE(ret == 0, "[connect] errno=" << errno << "\tret=" << ret << "\tfd=" << fd);

        auto conn = std::make_shared<TestConn>(eventloop, fd, addr, 1000);
        auto err = conn->Run();
        if (err != std::nullopt)
            BOOST_ERROR(err->What());

        err = conn->Send(bbt::core::Buffer{msg});
        if (err != std::nullopt)
            BOOST_ERROR(err->What());

        BOOST_TEST_MESSAGE("[client] send succ");
        ::sleep(1);
        ::close(fd);
        l.Down();
        BOOST_TEST_MESSAGE("[client] exit!");
    };

    l.Wait();
}

BOOST_AUTO_TEST_CASE(t_end)
{
    eventloop = nullptr;
}


BOOST_AUTO_TEST_SUITE_END()