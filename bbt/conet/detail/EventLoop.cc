#include <bbt/conet/detail/EventLoop.hpp>
#include <bbt/conet/detail/IOTask.hpp>

namespace bbt::network::conet::detail
{

EventLoop::EventLoop(size_t copool_size, bool auto_dispatch):
    m_auto_dispatch(auto_dispatch)
{
    g_scheduler->Start(m_auto_dispatch ? bbt::coroutine::SCHE_START_OPT_SCHE_THREAD : bbt::coroutine::SCHE_START_OPT_SCHE_NO_LOOP);
    m_co_pool = new bbt::coroutine::pool::CoPool(copool_size);
}

EventLoop::~EventLoop()
{
    m_co_pool->Release();
    delete m_co_pool;
    m_co_pool = nullptr;
    g_scheduler->Stop();
}

EventId EventLoop::RegistEvent(std::shared_ptr<interface::IConnection> udata, int events, int timeout, const OnDispatchCallback& callback)
{
    int socket = udata ? udata->GetFd() : -1;

    auto* task = new IOTask{[=](std::shared_ptr<interface::IConnection> conn, short event){
        callback(conn, event);
    }};

    std::lock_guard<std::mutex> lock{m_task_map_mtx};
    auto [_, succ] = m_task_map.insert(std::make_pair(task->GetId(), task));
    Assert(succ);

    EventId id = task->GetId();

    /**
     * 这个事件是一定会触发的（除非event设置的有问题）
     */
    __bbtco_event_regist_ex(
        socket,
        events,
        timeout
    ) [=](int fd, short event){
        Assert(this->m_co_pool->Submit([=](){
            task->Invoke(udata, event);
            _OnEvent(task->GetId());
        }) == 0);
    };

    return id;
}

int EventLoop::UnRegistEvent(EventId id)
{
    if (id <= 0)
        return -1;
    
    std::unique_lock<std::mutex> lock{m_task_map_mtx};
    auto it = m_task_map.find(id);
    if (it == m_task_map.end())
        return -1;
    
    return it->second->Cancel();
}

void EventLoop::Dispatch()
{
    g_scheduler->LoopOnce();
}

bool EventLoop::HasEvent(EventId id)
{
    std::unique_lock<std::mutex> lockf{m_task_map_mtx};
    auto it = m_task_map.find(id);
    return (it != m_task_map.end());
}

void EventLoop::_OnEvent(EventId id)
{
    std::unique_lock<std::mutex> lock{m_task_map_mtx};
    auto it = m_task_map.find(id);
    auto* task = it->second;
    Assert(m_task_map.erase(id) > 0);

    delete task;
}



}