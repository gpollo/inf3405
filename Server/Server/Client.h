#ifndef CLIENT_H
#define CLIENT_H

#include "Utils.h"
#include "Server.h"
#include "Message/Buffer.h"
#include "Message/ServerText.h"

#ifdef __LINUX__
    #include <sys/socket.h>
#endif
#ifdef __WIN32__
    #include <winsock2.h>
#endif
#include <iostream>
#include <deque>
#include <thread>
#include <mutex>

class Logger {
public:
    /**
     * Default constructor.
     */
    Logger() {

    }

    /**
     * The constructor by values.
     *
     * @param prefix The prefix of the logger.
     */
    Logger(const std::string prefix){
        prefix_ = prefix;
    }

    /**
     * The destructor.
     */
    ~Logger() {

    }

    /************
     * Mutators * 
     ************/

    /**
     * This method sets the prefix of the logger.
     *
     * @param prefix The prefix of the logger.
     */
    void setPrefix(const std::string& prefix) {
        prefix_ = prefix;
    }

    /**********************
     * Overloaded methods *
     **********************/

    void operator()(const std::string& os) {
        std::cout << prefix_ << os << std::endl;
    }

private:
    /** The prefix of the logger. */
    std::string prefix_;
};

/* server class prototype */
class Server;

class Client {
public:
    /**
     * The value constructor.
     *
     * @param server The server that the client belongs to.
     * @param socket The socket of the client.
     */
    Client(Server* server, socket_t socket);

    /**
     * The destructor.
     */
    ~Client();

    /**
     * This method handles the client requests.
     */
    void handleClient();

    /**
     * This method sends data to the remote user.
     *
     * @param obj The object to send.
     *
     * @return If the message was sent successfully, 'true', else 'false'.
     */
    bool sendMessage(const SerializableObject& obj);


    void queue(MessageServerText* msg);

    /*************
     * Accessors *
     *************/

    /**
     * This method returns the name of the user. If the user isn't
     * authentificated, his name will be empty.
     *
     * @return The name of the client.
     */
    std::string getName() const;

    /**
     * This method returns the thread ID.
     *
     * @return The thread ID.
     */
    tid_t getThreadID() const;

    /**
     * This method returns the thread handle.
     *
     * @return The thread handle.
     */
    const std::thread& getThread() const;

    uint32_t getSocketAddr() const;

    uint32_t getSocketPort() const;

    bool isAuth() const;

private:
    /** The name of the user connected. */
    std::string name_;

    /** The server it belongs to. */
    Server* server_;

    /* The socket of the client. */
    socket_t socket_;

    struct sockaddr addr_;

    /** The thread of the client. */
    std::thread thread_;

    std::mutex mutex_;

    /** The buffer for reading data. */
    NetworkBuffer<BUFFER_SIZE,-1> buffer_;

    std::deque<MessageServerText*> queue_;

    SerializableObject* receiveMessage();

    Logger logger_;

    bool auth_;

    /**
     * This method waits for the client to authentificate.
     *
     * @return If the authentification was successful, `true`, else `false`.
     */
    bool waitAuthentification();

    /**
     * This method waits for the client to send a message. The resulting
     * message will be put inside the buffer. It assumes that the client is
     * already authentificate. If this method receives a MSG_QUIT type, it will
     * return `false`.
     *
     * @return If a message was received, `true`, else `false`.
     */
    bool waitMessage();

    bool sendQueuedMessages();
};

#endif
