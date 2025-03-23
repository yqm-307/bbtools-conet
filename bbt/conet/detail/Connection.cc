#include <bbt/coroutine/coroutine.hpp>
#include <bbt/conet/detail/Connection.hpp>
#include <bbt/conet/detail/EventLoop.hpp>


namespace bbt::network::conet::detail
{

int64_t Connection::_GenId()
{
    static std::atomic_int64_t _id{0};
    return (++_id);
}

Connection::Connection(std::shared_ptr<TIEventLoop> evloop, int fd, const IPAddress& addr, int timeout):
    m_event_loop(evloop),
    m_conn_id(_GenId()),
    m_socket(fd),
    m_timeout(timeout),
    m_last_active_time(bbt::core::clock::now<>()),
    m_input_buffer(new char[m_input_buffer_len])
{
}

Connection::~Connection()
{
    delete[] m_input_buffer;
}

std::optional<Errcode> Connection::Run()
{
    if (IsClosed())
        return Errcode{BBT_CONET_MODULE_NAME "connection is closed!", 0};

    auto pthis = shared_from_this();
    if (pthis == nullptr)
        return Errcode{BBT_CONET_MODULE_NAME "please use std::shared_ptr instead of raw pointer!", 0};

    bbtco [pthis](){
        pthis->_OnMainEvent();
    };

    return std::nullopt;
}

void Connection::OnTimeout()
{

}

bool Connection::IsClosed() const
{
    return (m_run_status >= CONN_CLOSE);
}

int Connection::GetFd() const
{
    return m_socket;
}

const IPAddress& Connection::GetPeerAddr() const
{
    return m_peer_addr;
}

int64_t Connection::GetId() const
{
    return m_conn_id;
}

void Connection::Close()
{
    /**
     * 如果有正在进行的发送事件，设置连接状态，等发送事件完成后再关闭连接
     * 如果没有正在进行的发送事件，直接Shutdown
     */

    std::unique_lock<std::mutex> lock(m_mutex);
    if (IsClosed())
        return;

    if (!m_send_event_is_in_progress && m_send_event < 0) {
        _Shutdown();
        lock.unlock();
        OnClose();
        return;
    }

    m_run_status = CONN_CLOSE;
}

void Connection::Shutdown()
{
    std::unique_lock<std::mutex> lock{m_mutex};
    _Shutdown();
    lock.unlock();
    OnClose();
}

void Connection::_Shutdown()
{
    if (m_run_status == CONN_DISCONNECTED)
        return;

    m_run_status = CONN_DISCONNECTED;

    Assert(m_socket >= 0);
    ::close(m_socket);
    m_socket = -1;
}


std::optional<Errcode> Connection::Send(const bbt::core::Buffer& buf)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (IsClosed()) {
        return Errcode{BBT_CONET_MODULE_NAME "connection is closed!", 0};
    }

    int append_len = _AppendOutputBuffer(buf.Peek(), buf.Size());
    if (append_len != buf.Size())
        return Errcode{BBT_CONET_MODULE_NAME "output buffer not enough!", 0};

    if (m_send_event_is_in_progress == true) {
        return std::nullopt;
    }

    return _RegistASendEvent();
}

int Connection::_OnSendEvent(std::shared_ptr<bbt::core::Buffer> buffer, short event)
{
    size_t len = 0;
    bool continue_send = false;

    std::unique_lock<std::mutex> lock{m_mutex};
    if (m_run_status == CONN_DISCONNECTED)
        return -1;


    if(event & bbtco_emev_writeable) {
        lock.unlock();
        OnSend(::send(m_socket, buffer->Peek(), buffer->Size(), MSG_NOSIGNAL));
        lock.lock();

        // Close状态或者output buffer没有数据，就不注册新的发送事件了
        continue_send = (m_run_status != CONN_DISCONNECTED && m_output_buffer.Size() > 0);
    }

    /**
     * 发送完毕后，若缓冲区还有数据且连接没有处于断开状态，继续注册发送事件
     * 若处于close状态，则Shutdown
     */
    m_send_event = -1;
    m_send_event_is_in_progress = false;

    if (!continue_send) {
        if (m_run_status == CONN_CLOSE) {
            _Shutdown();
            lock.unlock();
            OnClose();
        }
    } else {
        _RegistASendEvent();
    }

    return 0;
}


int Connection::Send(const char* byte, size_t len)
{
    return 0;
}

int Connection::_AppendOutputBuffer(const char* data, size_t len)
{
    auto before = m_output_buffer.Size();
    m_output_buffer.WriteString(data, len);
    auto after_size = m_output_buffer.Size();

    int change_num = after_size - before;

    return change_num > 0 ? change_num : 0;
}

std::optional<Errcode> Connection::_RegistASendEvent()
{
    AssertWithInfo(!m_send_event_is_in_progress, "output buffer must be free!");
    AssertWithInfo(m_send_event <= 0, "must no send event!");

    auto buffer_sptr = std::make_shared<bbt::core::Buffer>();
    buffer_sptr->Swap(m_output_buffer);

    auto pthis = shared_from_this();
    m_send_event_is_in_progress = true;
    m_send_event = m_event_loop->RegistEvent(shared_from_this(), bbtco_emev_writeable | bbtco_emev_finalize, -1,
    [pthis, buffer_sptr](auto, short event){
        pthis->_OnSendEvent(buffer_sptr, event);

        return false;
    });

    return std::nullopt;
}

void Connection::_OnMainEvent()
{
    while (!IsClosed())
    {
        // 等待io事件
        Assert(g_bbt_tls_coroutine_co->YieldUntilFdReadable(m_socket, m_timeout) == 0);
        auto event = g_bbt_tls_coroutine_co->GetLastResumeEvent();

        /* 处理事件 */
        if (event & bbtco_emev_close) { // socket 关闭了
            Close();
        } else if (event & bbtco_emev_timeout) {    // 连接超时了
            OnTimeout();
            Close();
        } else if (event & bbtco_emev_readable) {   // 可读
            auto err = _Recv();
            if (err.has_value() && err->Type() == network::ERRTYPE_NETWORK_RECV_EOF) {  // 读eof，对端关闭了
                Close();
            }
            else if (err)   // 其他错误
                OnError(err.value());
        } else {
            OnError(Errcode{BBT_CONET_MODULE_NAME "unknown event=" + std::to_string(event), 0});
        }
    }
}

ErrOpt Connection::_Recv()
{
    int err = 0;
    int read_len = 0;
    Errcode errcode{"", ERRTYPE_NOTHING};
    std::string err_msg{""};
    ErrType err_type;


    if (IsClosed()) {
        return Errcode{BBT_CONET_MODULE_NAME "conn is closed, but event was not cancel! peer:" + GetPeerAddr().GetIPPort(), 0};
    }

    read_len = bbt::co::detail::Hook_Read(m_socket, m_input_buffer, m_input_buffer_len);

    if (read_len == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            err_msg = BBT_CONET_MODULE_NAME "please try again!";
            err_type = ERRTYPE_NETWORK_RECV_TRY_AGAIN;
        } else if (errno == ECONNREFUSED) {
            err_msg = BBT_CONET_MODULE_NAME "connect refused!";
            err_type = ERRTYPE_NETWORK_RECV_CONNREFUSED;
        } else {
            err_msg = BBT_CONET_MODULE_NAME "other errno! errno=" + std::to_string(errno);
            err_type = ERRTYPE_NETWORK_RECV_OTHER_ERR;
        }
    } else if (read_len == 0) {
        err_msg = BBT_CONET_MODULE_NAME "peer connect closed!";
        err_type = ERRTYPE_NETWORK_RECV_EOF;
    } else if (read_len < -1) {
        err_msg = BBT_CONET_MODULE_NAME "read error! errno=" + std::to_string(errno);
        err_type = ERRTYPE_NETWORK_RECV_OTHER_ERR;
    }

    if (err_msg.size() > 0) {
        return Errcode{err_msg, err_type};
    }

    OnRecv(m_input_buffer, read_len);

    return std::nullopt;
}


}