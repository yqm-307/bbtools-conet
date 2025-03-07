#pragma once
#include <bbt/conet/detail/Define.hpp>
#include <bbt/conet/detail/interface/IConnection.hpp>

namespace bbt::network::conet::detail
{

class TcpClient
{
public:
    typedef ErrTuple<std::shared_ptr<interface::IConnection>> ConnectResult;

    explicit TcpClient(std::shared_ptr<TIEventLoop> eventloop):m_event_loop(eventloop) {}
    virtual ~TcpClient() {}

    /**
     * @brief 协程连接事件（异步连接事件）
     * 此接口会开启一个新的协程执行connect操作，并通过OnConnect通知结果
     * 
     * @param ip 对端地址
     * @param port 对端端口
     */
    virtual void                                    CoConnect(const std::string& ip, short port);

    /**
     * @brief 同步连接事件
     * 在协程中调用时，不会阻塞系统线程；但是在非协程环境中是阻塞调用
     * 
     * @param ip 对端地址
     * @param port 对端端口
     * @return ConnectResult 
     */
    virtual ConnectResult                           Connect(const std::string& ip, short port);
protected:

    /**
     * @brief 连接完成时触发
     * 需要由派生类重载此函数，此函数应该建立一个新连接并将错误信息和连接
     * 返回
     * 
     * @param socket 新连接的套接字
     * @param addr 对端地址
     * @return ConnectResult 
     */
    virtual ConnectResult                           OnConnect(int socket, const IPAddress& addr) = 0;

    /**
     * @brief 错误处理
     * 
     * @param err 
     */
    virtual void                                    OnError(const Errcode& err) = 0;
    virtual std::shared_ptr<TIEventLoop>            GetEventLoop() final;
private:
    ConnectResult                                   _ConnectCo(const std::string& ip, short port);
private:
    std::weak_ptr<TIEventLoop>                      m_event_loop;
};


}