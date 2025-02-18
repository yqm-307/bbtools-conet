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
    m_last_active_time(bbt::clock::now<>()),
    m_input_buffer(new char[m_input_buffer_len])
{
    AssertWithInfo(evloop != nullptr, "need a eventloop!");
}

Connection::~Connection()
{
    delete[] m_input_buffer;
}

std::optional<Errcode> Connection::Run()
{
    if (IsClosed())
        return Errcode{"connection is closed!"};

    auto pthis = shared_from_this();
    if (pthis == nullptr)
        return Errcode{"please use std::shared_ptr instead of raw pointer!"};

    auto eventloop = _GetEventLoop();
    if (eventloop == nullptr)
        return Errcode{"eventloop is released!"};

    m_main_event = eventloop->RegistEvent(shared_from_this(),
        bbtco_emev_close |
        bbtco_emev_readable,
        m_timeout,
        [pthis](auto, short events){ return pthis->_OnMainEvent(events); }
    );

    if (m_main_event <= 0)
        return Errcode{"eventloop regist connect timeout event failed!"};

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
        return Errcode{"connection is closed!"};
    }

    bool not_in_progress = false;
    int append_len = _AppendOutputBuffer(buf.Peek(), buf.DataSize());
    if (append_len != buf.DataSize())
        return Errcode{"output buffer not enough!"};

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
        OnSend(::send(m_socket, buffer->Peek(), buffer->DataSize(), MSG_NOSIGNAL));
        lock.lock();

        // Close状态或者output buffer没有数据，就不注册新的发送事件了
        continue_send = (m_run_status != CONN_DISCONNECTED && m_output_buffer.DataSize() > 0);
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
    auto before = m_output_buffer.DataSize();
    m_output_buffer.WriteString(data, len);
    auto after_size = m_output_buffer.DataSize();

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
    auto eventloop = _GetEventLoop();
    if (eventloop == nullptr)
        return Errcode{"eventloop is released!"};
    
    m_send_event_is_in_progress = true;
    m_send_event = eventloop->RegistEvent(shared_from_this(), bbtco_emev_writeable | bbtco_emev_finalize, -1,
    [pthis, buffer_sptr](auto, short event){
        pthis->_OnSendEvent(buffer_sptr, event);

        return false;
    });

    return std::nullopt;
}

std::optional<Errcode> Connection::_RegistAMainEvent()
{
    auto pthis = shared_from_this();
    if (pthis == nullptr)
        return Errcode{"please use std::shared_ptr instead of raw pointer!"};

    auto eventloop = _GetEventLoop();
    if (eventloop == nullptr)
        return Errcode{"eventloop is released!"};

    m_main_event = eventloop->RegistEvent(shared_from_this(),
        bbtco_emev_close |
        bbtco_emev_readable,
        m_timeout,
        [=](auto, short events){ return pthis->_OnMainEvent(events); }
    );

    if (m_main_event <= 0)
        return Errcode{"eventloop regist connect timeout event failed!"};

    return std::nullopt;
}


std::shared_ptr<EventLoop> Connection::_GetEventLoop()
{
    return std::dynamic_pointer_cast<EventLoop>(m_event_loop.lock());
}

bool Connection::_OnMainEvent(short event)
{
    /* 若已经关闭了，释放此事件 */
    if (IsClosed()) {
        auto eventloop = _GetEventLoop();
        if (eventloop)
            eventloop->UnRegistEvent(m_main_event);
        
        m_main_event = -1;
        return false;
    }


    /* 处理事件 */
    if (event & bbtco_emev_close) {
        Close();
    } else if (event & bbtco_emev_timeout) {
        OnTimeout();
        Close();
    } else if (event & bbtco_emev_readable) {
        auto err = _Recv();
        if (err && err->IsErr() && err->Type() == network::ERRTYPE_NETWORK_RECV_EOF)
            Close();
        else if (err)
            OnError(err.value());
        else
            _RegistAMainEvent();
    } else {
        OnError(Errcode{"unknown event=" + std::to_string(event)});
    }

    return false;
}

std::optional<Errcode> Connection::_Recv()
{
    int err = 0;
    int read_len = 0;
    Errcode errcode{"", ERRTYPE_NOTHING};


    if (IsClosed()) {
        return Errcode{"conn is closed, but event was not cancel! peer:" + GetPeerAddr().GetIPPort()};
    }

    read_len = ::read(m_socket, m_input_buffer, m_input_buffer_len);

    if (read_len == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            errcode = Errcode{"please try again!", ERRTYPE_NETWORK_RECV_TRY_AGAIN};
        } else if (errno == ECONNREFUSED) {
            errcode = Errcode{"connect refused!", ERRTYPE_NETWORK_RECV_CONNREFUSED};
        } else {
            errcode = Errcode{"other errno! errno=" + std::to_string(errno), ERRTYPE_NETWORK_RECV_OTHER_ERR};
        }
    } else if (read_len == 0) {
        errcode = Errcode{"peer connect closed!", ERRTYPE_NETWORK_RECV_EOF};
    } else if (read_len < -1) {
        errcode = Errcode{"other errno! errno=" + std::to_string(errno), ERRTYPE_NETWORK_RECV_OTHER_ERR};
    }

    if (errcode.IsErr()) {
        return errcode;
    }

    OnRecv(m_input_buffer, read_len);

    return std::nullopt;
}


}