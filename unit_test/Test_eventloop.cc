#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>

#include <bbt/conet/detail/EventLoop.hpp>

BOOST_AUTO_TEST_SUITE()

bbt::network::conet::detail::EventLoop* eventloop = nullptr;

BOOST_AUTO_TEST_CASE(t_begin)
{
    eventloop = new bbt::network::conet::detail::EventLoop(1000, true);
}

BOOST_AUTO_TEST_CASE(t_event_notify)
{
    int flag = 0;
    eventloop->RegistEvent(nullptr, bbtco_emev_timeout, 100, [&](auto null, short ev){
        BOOST_ASSERT(null == nullptr);
        BOOST_ASSERT(ev == bbtco_emev_timeout);
        flag = 1;
        return false;
    });

    std::this_thread::sleep_for(bbt::core::clock::ms(200));
    BOOST_CHECK(flag == 1);
}

BOOST_AUTO_TEST_CASE(t_cancel_event)
{

    bbt::core::thread::CountDownLatch latch{1};

    bbtco [&](){
        auto id = eventloop->RegistEvent(nullptr, bbtco_emev_timeout, 100, [&](auto null, short ev){
            BOOST_ASSERT(false);
            return false;
        });
    
        BOOST_ASSERT(id > 0);
    
        BOOST_ASSERT(eventloop->UnRegistEvent(id) == 0);
        BOOST_ASSERT(eventloop->UnRegistEvent(id) == -1);
        bbtco_sleep(200);
        BOOST_ASSERT(eventloop->UnRegistEvent(id) == -1);
        latch.Down();
    };

    latch.Wait();
}

BOOST_AUTO_TEST_CASE(t_end)
{
    delete eventloop;
}

BOOST_AUTO_TEST_SUITE_END()