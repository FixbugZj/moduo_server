#include"redis.hpp"
#include<iostream>
#include<thread>
using namespace std;

Redis::Redis()
        :_publish_context(nullptr)
        ,_subscribe_context(nullptr)
{

}
Redis::~Redis()
{
        if(_publish_context != nullptr)
        {
            redisFree(_publish_context);
        }
        if(_subscribe_context != nullptr)
        {
            redisFree(_subscribe_context);
        }
}

//连接redis服务器
bool Redis::connect()
{
    //负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1",6379);
    if(_publish_context == nullptr)
    {
        cerr<<"connect redis faild"<<endl;
        return false;
    }

    //负责subscribe发布消息的上下文连接
    _subscribe_context=redisConnect("127.0.0.1",6379);
    if(_subscribe_context == nullptr)
    {
        cerr<<"connect redis faild!"<<endl;
        return false;
    }

    //在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    thread t([&](){
        observer_channel_message();

    });
    t.detach();

    cout<<"connect redis-server success!"<<endl;
    return true;
}

//向redis指定的通道channel发布消息
bool Redis::publish(int channer,string message)
{
    redisReply *reply = (redisReply*)redisCommand(_publish_context,"PUBLISH %d %s",channer,message.c_str());
    if(reply == nullptr)
    {
        cerr<<"publish connand faild!"<<endl;
        return false;
    }
    freeReplyObject(reply);
    return true;

}

//向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context,"SUBSCRIBE %d",channel))
    {
        cerr<<"subscribe command failed!"<<endl;
        return false;
    }

    int done=0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context,&done))
        {
            cerr<<"subscribe command failed!"<<endl;
            return false;
        }

    }
    return true;
}

//向redis指定的通道ubsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context,"UNSUBSCRIBE %d",channel))
    {
        cerr<<"unsubscribe command failed!"<<endl;
        return false;
    }

    int done=0;
    while(!done)
    {
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context,&done))
        {
            cerr<<"unsubscribe command failed!"<<endl;
            return false;
        }

    }
    return true;
}

//在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subscribe_context,(void**)&reply))
    {
        //订阅收到的消息是一个带三元素的数据
        if(reply!= nullptr && reply->element[2] != nullptr && reply->element[2]->str !=nullptr)
        {
            //给业务层上报通道发生消息
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }

    cerr<<">>>>>>>>>>>>>>>>>pbserver_channel_message quit <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"<<endl;
}

//初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int,string)> fn)
{
    this->_notify_message_handler = fn;
}