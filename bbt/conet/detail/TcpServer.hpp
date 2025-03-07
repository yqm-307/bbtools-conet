#pragma once
#include <bbt/conet/detail/Define.hpp>

namespace bbt::network::conet::detail
{

class TcpServer
{
public:
    explicit TcpServer(std::shared_ptr<TIEventLoop> loop, const std::string& ip, short port);
    virtual ~TcpServer();

    /**
     * @brief 监听阻塞启动
     * 
     */
    std::optional<Errcode>                  Start();

    /**
     * @brief 监听协程启动（非阻塞）
     * 
     */
    void                                    CoStart();

    /**
     * @brief 停止监听
     * 
     * @param sync 是否同步等待监听者退出
     */
    void                                    Stop(bool sync = false);

    virtual std::shared_ptr<TIEventLoop>            GetEventLoop() final;
protected:
    /**
     * @brief 错误处理（继承类自定义该行为）
     * 
     * @param err 
     */
    virtual void                            OnError(const Errcode& err) = 0;

    /**
     * @brief 监听到新连接触发（继承类自定义该行为）
     * 若accept失败，会通过OnError通知错误。通过OnAccept的都是成功accept的连接
     * 
     * @param socket 套接字
     * @param addr 对端地址
     */
    virtual void OnAccept(int socket, const IPAddress& addr) = 0;
private:
    virtual void _ListenCo();
private:
    std::weak_ptr<TIEventLoop>      m_event_loop;
    const IPAddress                 m_listen_addr;
    volatile bool                   m_is_running{true};
    bbt::core::thread::CountDownLatch*    m_latch;

};

}