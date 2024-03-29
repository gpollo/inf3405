#include <iostream>

#include "Utils.h"
#include "Server.h"
#include "Message/ServerText.h"

#ifdef __LINUX__
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <errno.h>
    #include <string.h>
    #include <unistd.h>
    #include <poll.h>
#endif
#ifdef __WIN32__
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <windows.h>
#endif
#include <exception>
#include <system_error>
#include <time.h>
#include <algorithm>

using namespace std;

Server::Server(const string& db, const string& addr, uint_t port) :
    addr_(addr),
    port_(port),
    db_(db) {

    /* the server isn't running yet */
    running_ = false;

    /* make sure the port is valid */
    if(!isPortValid(port))
        throw invalid_argument("the specified port isn't valid");

    /* create the structure for the socket address */
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port);

    /* convert the address */
    service.sin_addr.s_addr = inet_addr(addr.c_str());
    if(service.sin_addr.s_addr == INADDR_NONE)
        throw invalid_argument("the specified address isn't valid");

    /* load the database */
    if(!db_.init())
        throw invalid_argument("could not load the database file");

    /* create the listening socket */
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef __LINUX__
    if(socket_ < 0)
        throw runtime_error("socket() failed: error " + to_string(errno));
#endif
#ifdef __WIN32__
    if(socket_ == INVALID_SOCKET) {
        int code = WSAGetLastError();
        WSACleanup();
        throw runtime_error("socket() failed: error " + to_string(code));
    }
#endif

    /* bind the listening socket to the address */
    int ret = ::bind(socket_, (sockaddr*)& service, sizeof(service));
#ifdef __LINUX__
    if(ret < 0) {
        close(socket_);
        throw runtime_error("bind() failed: error " + to_string(errno));
    }
#endif
#ifdef __WIN32__
    if (ret == SOCKET_ERROR) {
        int code = WSAGetLastError();
        closesocket(socket_);
        WSACleanup();
        throw runtime_error("bind() failed: error " + to_string(code));
    }
#endif

    /* set the socket as listening */
    ret = listen(socket_, 1);
#ifdef __LINUX__
    if(ret < 0) {
        close(socket_);
        throw runtime_error("listen() failed: error " + to_string(errno));
    }
#endif
#ifdef __WIN32__
    if (ret == SOCKET_ERROR) {
        int code = WSAGetLastError();
        closesocket(socket_);
        WSACleanup();
        throw runtime_error("listen() failed: error " + to_string(code));
    }
#endif
}

Server::~Server() {
	/* close the socket */
#ifdef __LINUX__
	close(socket_);
#endif
#ifdef __WIN32__
	closesocket(socket_);
#endif

	/* save the database */
	cout << "Saving the database..." << endl;
	db_.save();
}

void Server::waitConnexion() {
    cout << "Starting the server..." << endl;

    /* configure the structure for polling the socket */
    struct pollfd pollsocket;
    pollsocket.fd     = socket_;
    pollsocket.events = POLLRDNORM;

    /* set the server as running */
    running_ = true;

    /* continuously listen for incoming connections */
    while(running_) {
        /* wait for new connexions */
#ifdef __LINUX__
        int ret = poll(&pollsocket, 1, 1);
#endif
#ifdef __WIN32__
        int ret = WSAPoll(&pollsocket, 1, 1);
#endif
		/* poll timeout */
		if(ret == 0) continue;

		/* poll error */
        if(ret < 0) {
            /* if a signal was caught, it might be CTRL+C */
#ifdef __LINUX__
            if(errno == EINTR)
                continue;
#endif
#ifdef __WIN32__
            if(WSAGetLastError() == WSAEINTR)
                continue;
#endif
            
            /* poll failed for some reason */
#ifdef __LINUX__
            throw runtime_error("poll() failed: error " + to_string(errno));
#endif
#ifdef __WIN32__
            throw runtime_error("WSAPoll() failed: error " + to_string(WSAGetLastError()));
#endif
        }

        /* connect to the new client */
        socket_t socket = accept(socket_, NULL, NULL);
#ifdef __LINUX__
        if(socket < 0)
            throw runtime_error("accept() failed: error" + to_string(errno));
#endif
#ifdef __WIN32__
        if(socket == INVALID_SOCKET)
            throw runtime_error("accept() failed: error" + to_string(WSAGetLastError()));
#endif

        /* create a new thread for the client */
        Client* client = new Client(this, socket);

        /* add the new client into the list */
		mutex_.lock();
        clients_.push_back(client);
		mutex_.unlock();
    }
}

void Server::unregister(Client* client) {
	mutex_.lock();
	clients_.erase(std::remove(clients_.begin(), clients_.end(), client), clients_.end());
	mutex_.unlock();
}

void Server::stop() {
    /* shutdown the server */
    if(running_)
        cout << "\rGracefully shutting down the server..." << endl;

    /* set the server as stopped */
    running_ = false;
}

bool Server::authentificate(const string& name, const string& pass) {
    /* check if the user exists */
    User* user = db_.getUser(name);

    /* create the user if it doesn't exist */
    if(user == nullptr)
        return (db_.addUser(name, pass) !=  nullptr);

    /* check is the password is valid */
    if(!user->validatePass(pass))
        return false;

    /* check if another client is connected with that account */
    for(Client* c : clients_) {
        /* make sure the client is authentificated */
        if(c->isAuth())
            continue;

        /* prevent null pointers */
        if(c == nullptr)
            continue;

        /* check if the username isn't used */
        if(c->getName() == name)
            return false;
    }

    return true;
}

void Server::getBacklog(Client* client) {
    vector<MessageServerText*> backlog = db_.getBacklog();

    mutex_.lock();
    for(MessageServerText* text : backlog) {
        client->queue(text);
    }
    mutex_.unlock();
}

bool Server::sendText(Client* client, const string& msg) {
    /* lock the mutex since we only handle one message at a time */
    mutex_.lock();

    /* get message informations */
    time_t timestamp = time(0);
    uint32_t addr = client->getSocketAddr();
    uint32_t port = client->getSocketPort();
    string username = client->getName();

    /* create the server message */
    MessageServerText* text;
    text = new MessageServerText(username, timestamp, addr, port, msg);

    /* we send the message to other client */
    for (Client* c : clients_) {
        /* make sure the client is authentificated */
        if(!c->isAuth())
            continue;

        /* send the message to the other clients, including the sender */
        c->queue(text);
    }

    /* write the new message into the backlog */
    db_.addMsg(text);

    /* release the mutex */
    mutex_.unlock();

    return true;
}
