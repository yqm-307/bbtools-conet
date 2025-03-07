#include <bbt/conet/conet.hpp>
#include "echoconnection.hpp"

class EchoMultiClient:
    public bbt::conet::TcpClient
{
public:

    EchoMultiClient(std::shared_ptr<bbt::conet::detail::EventLoop> eventloop):bbt::conet::TcpClient(eventloop) {}
    virtual ~EchoMultiClient() {}

    virtual ConnectResult OnConnect(int socket, const bbt::conet::IPAddress& addr)
    {
        auto conn = std::make_shared<EchoConn<false>>(GetEventLoop(), socket, addr, 1000);
        conn->Run();
        bbtco [conn]{
            for (int i = 0; i < 10000; ++i) {
                conn->Send(bbt::core::Buffer{"hello world"});
            }

            // conn->Close();
        };

        return {std::nullopt, conn};
    }

    virtual void OnError(const bbt::network::Errcode& err)
    {
        printf("[echo serv] [error] %s\n", err.CWhat());
    }
};

int main()
{
    auto eventloop = std::make_shared<bbt::conet::detail::EventLoop>(100, true);

    EchoMultiClient client{eventloop};

    while (true) {
        client.CoConnect("127.0.0.1", 10101);
        std::this_thread::sleep_for(bbt::core::clock::ms(100));
    }
}