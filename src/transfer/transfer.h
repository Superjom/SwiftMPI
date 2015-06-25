//
//  one.h
//  SwiftSnails
//
//  Created by Chunwei on 3/11/15.
//  Copyright (c) 2015 Chunwei. All rights reserved.
//
#ifndef SwiftSnails_transfer_transfer_h_
#define SwiftSnails_transfer_transfer_h_
#include "../utils/all.h"
#include "./Message.h"
#include "Listener.h"
#include "ServerWorkerRoute.h"

namespace swift_snails {

/**
 * each message_class is mapped to a handler
 *
 * when server receive a Message, 
 * it will handle the message with the corressponding message_class's handler
 */
template<typename Handler>
class MessageClass :public VirtualObject {
public:
    /** **TODO** change spinlock to read-write-lock ? 
     *
     * do not change now , because message_class will only be changed 
     * before read
     */
    void add (index_t id, Handler&& handler) noexcept {
        LOG(INFO) << "register message class: " << id;
        std::lock_guard<SpinLock> lock(_spinlock);
        is_writing = true;
        CHECK(_message_classes.count(id) == 0) <<
                "callback should be registerd only once";
        _message_classes.emplace(id, std::move(handler));
        is_writing = false;
    }

    void remove (index_t id) noexcept {
        std::lock_guard<SpinLock> lock(_spinlock);
        is_writing = true;
        auto pos = _message_classes.find(id);
        CHECK(pos != _message_classes.end()) <<
                "no message_class:" << id << " found!";
        _message_classes.erase(pos);
        is_writing = false;
    }

    Handler& get (index_t id) noexcept {
        auto it = _message_classes.find(id); 
        CHECK(it != _message_classes.end());
        return it->second;
    }

private:
    SpinLock _spinlock;
    std::map<index_t, Handler> _message_classes;
    bool is_writing = false;
    //std::condition_variable _cond;
    //std::mutex _mut;
};  // end MessageClass


template<typename Route>
class Transfer : public Listener {
public:

    // message class handler
    typedef std::function<void(std::shared_ptr<Request>, Request&)> msgcls_handler_t;
    // message respons callback handler
    typedef Request::response_call_back_t   msgrsp_handler_t;


    explicit Transfer() : 
        _route( global_route())
    {
        set_zmq_ctx(_route.zmq_ctx());
    }
    // init later
    void init_async_channel(int thread_num) noexcept {
        CHECK(!_async_channel) << "async channel has been created";
        AsynExec as(thread_num);
        _async_channel = as.open();
    }
    void set_client_id(int client_id) noexcept {
        _client_id = client_id;
    }
    /**
     * \param request request
     * \param to_id  id of the node where the message is sent to
     */
    void send(Request &&request, int to_id) noexcept {
        index_t msg_id = _msg_id_counter++;
        request.set_msg_id(msg_id);
        CHECK(_client_id != -3) << "shoud set client_id first";

        if(client_id() >= 0) {
            request.meta.client_id = _client_id;
        }
        // convert Request to underlying Package
        Package package(request);
        //LOG(INFO) << "send package";
        // cache the recall_back
        // when the sent message's reply is received 
        // the call_back handler will be called
        { std::lock_guard<SpinLock> lock(_msg_handlers_mut);
            //LOG(INFO) << "to register call_back_handler";
            CHECK(_msg_handlers.emplace(msg_id, std::move(request.call_back_handler)).second);
            //LOG(INFO) << "call_back_handler is registered";
        }

        CHECK(_route.send_addrs().count(to_id) > 0) 
            << "no node_id " << to_id << " in the route";

        // send the package
        Route& route = _route;

        {
            //LOG(INFO) << "to lock send_mutex";
            std::lock_guard<std::mutex> lock(
                * route.send_mutex(to_id)
            );
            //LOG(INFO) << "zmq to send message";
            PCHECK(ignore_signal_call(zmq_msg_send, &package.meta.zmg(), route.sender(to_id), ZMQ_SNDMORE) >= 0);
            PCHECK(ignore_signal_call(zmq_msg_send, &package.cont.zmg(), route.sender(to_id), 0) >= 0);
        }
    }
    /**
     * \brief start service threads
     * \warning should run as a thread
     *  receive request  and run corresponding message-class-handler
     *  receive reply message, read the reply message and run the correspondding handler
     */
    void main_loop() noexcept {
        Package package;
        Request::response_call_back_t handler;
        for(;;) {

            { std::lock_guard<std::mutex> lock(receiver_mutex() );
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.meta.zmg(), receiver(), 0) >= 0);
                if(package.meta.size() == 0) break;
                CHECK(zmq_msg_more(&package.meta.zmg()));
                PCHECK(ignore_signal_call(zmq_msg_recv, &package.cont.zmg(), receiver(), 0) >= 0);
                CHECK(!zmq_msg_more(&package.cont.zmg()));
            }

            std::shared_ptr<Request> request = std::make_shared<Request>(std::move(package));

            if(request->is_response()) {
                RAW_DLOG(INFO, "receive a response, message_id: %d", request->meta.message_id);
                handle_response(request);
            } else {
                RAW_DLOG(INFO, "receive a request, message_class: %d, client_id: %d", request->meta.message_class, request->meta.client_id);
                handle_request(request);
            }
        }
        LOG(WARNING) << "sender terminated!";
    }

    /** handle the request from other node
     * and run message_class-handler
     */
    void handle_request(std::shared_ptr<Request> request) noexcept {

        msgcls_handler_t handler = _message_class.get( request->meta.message_class);
        CHECK(!_async_channel->closed());
        //LOG(INFO) << "push task to channel";
        _async_channel->push(
            [this, handler, request] {
                Request response;
                handler(request, response);
                // set response meta
                response.meta.message_id = request->meta.message_id;
                response.meta.client_id = request->meta.client_id;
                //response.meta.message_class = request->meta.message_class;
                // response flag
                response.meta.message_class = -1;

                RAW_DLOG(INFO, "send response to client %d", request->meta.client_id);
                // only send response with content
                // empty response will not be sent, and master should
                // send a response with content later
                if(response.cont.size() > 0) {
                    send_response(std::move(response),  request->meta.client_id);
                } else {
                    RAW_DLOG(INFO, "empty response, not send");
                }
            }
        );
    }
    
    /** handle the response from other node
     * and run response-callback handler
     */
    void handle_response(std::shared_ptr<Request> &response) noexcept {
        Request::response_call_back_t handler;
        // NOTE: allow client_id == 0 , when the cluster's route has not been created
        CHECK((_client_id >=-1 && _client_id <= 0) || response->meta.client_id == client_id()) 
                << "get client_id\t" << response->meta.client_id
                << "\tlocal_id\t" << _client_id;

        //LOG(INFO) << ".. call response handler";
        // call the callback handler
        { std::lock_guard<SpinLock> lock(_msg_handlers_mut);
            auto it = _msg_handlers.find(response->message_id());
            CHECK(it != _msg_handlers.end());
            handler = std::move(it->second);
            _msg_handlers.erase(it);
        }

        //LOG(INFO) << ".. push response handler to channel";

        // execute the response_recallback handler
        _async_channel->push(
            // TODO refrence handler?
            [handler, this, response]() {
                handler(response);
            }
        );
    }

    void send_response(Request &&request, int to_id) noexcept {
        CHECK_GT( global_route().send_addrs().count(to_id), 0) 
            << "to_id(" << to_id << ") is not valid";
        request.meta.client_id = to_id;
        Package package(request);
        Route& route = _route;
        {
            // TODO will the mutex share between sender and receiver 
            // effect performance?
            std::lock_guard<std::mutex> lock(
                * route.send_mutex(to_id)
            );
            PCHECK(ignore_signal_call(zmq_msg_send, &package.meta.zmg(), route.sender(to_id), ZMQ_SNDMORE) >= 0);
            PCHECK(ignore_signal_call(zmq_msg_send, &package.cont.zmg(), route.sender(to_id), 0) >= 0);

            //LOG(INFO) << "response has been sent";
        }
    }

    int client_id() const noexcept {
        return _client_id;
    }
    Route& route() noexcept {
        return _route;
    }
    std::shared_ptr<AsynExec::channel_t>& async_channel() noexcept {
        return _async_channel;
    }
    // determine whether all sended message get a 
    // reply
    // Attention: not thread safe!
    bool service_complete() noexcept {
        return _msg_handlers.empty();
    }
    /**
     * work as an API
     *
     * control Receiver service by adding handler to message_class
     */
    MessageClass<msgcls_handler_t>& message_class() noexcept {
        return _message_class;
    }

    ~Transfer() {
        CHECK(service_complete());

        service_end();

        LOG(WARNING) << "transfer listener exit!";
        PCHECK(0 == zmq_close(_receiver));
        _receiver = NULL;

        //_async_channel->close();
    }

private:
    
    Route &_route;

    std::atomic<index_t> _msg_id_counter{0};
    std::shared_ptr<AsynExec::channel_t> _async_channel;
    std::map<index_t, Request::response_call_back_t> _msg_handlers;

    //SpinLock    _send_mut;
    SpinLock    _msg_handlers_mut;
    MessageClass<msgcls_handler_t> _message_class;

    int _client_id = -2;

};  // end class Transfer


};  // end namespace swift_snails
#endif
