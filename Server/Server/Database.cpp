#include "Utils.h"
#include "Database.h"
#include "Message/Buffer.h"
#include "Message/Types.h"

#include <iostream>
#include <fstream>

#ifdef __LINUX__
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif
#ifdef __WIN32__
    #include <windows.h>
	#include <shlwapi.h>
#endif

using namespace std;

bool fileExists(const string& path);

Database::Database(const string& file) :
    file_(file) {
}

Database::~Database() {
    /* delete the messages */
    for(MessageServerText* text : backlog_)
        delete text;

    /* delete the users */
    for(User* user : users_)
        delete user;
}

bool Database::init() {
	/* start with an empty database if the file doesn't exist */
	if(!fileExists(file_)) {
		cout << "Database file \'" << file_ << "\' doesn't exist, creating a new database." << endl;
		return true;
	}

	/* try to load the database file */
	if(!load()) {
		cout << "Failed to load database file \'" << file_ << "\'." << endl;
		return false;
	}

	/* the database has been loaded */
	cout << "Loaded database file \'" << file_ << "\'." << endl;
	return true;
}

bool Database::load() {
    /* open the file */
    FILE* fd = fopen(file_.c_str(), "r");
    if(fd == nullptr) {
        cout << "The database file could not be read." << endl;
        return false;
    }

    /* create a buffer for reading the file */
    FileBuffer<BUFFER_SIZE,-1> buffer(fd);

    while(true) {
        /* read an object */
        SerializableObject* obj = buffer.getMessage();
        if(obj == nullptr)
            break;

        /* add the object into the database */
        bool status = addSerializableObject(obj);

        /* delete the object if it wasn't added into the database */
        if(status == false)
            delete obj;
    }
    
    /* close the file */
    fclose(fd);

    return true;
}

bool Database::save() const {
    uint8_t buffer[BUFFER_SIZE];

    /* open the file */
	ofstream file(file_);
	if (!file.is_open()) {
		cout << "The file couldn't be opened." << endl;
		return false;
	}

    /* write the backlog */
    for(MessageServerText* text : backlog_) {
        int len = text->serialize(buffer, BUFFER_SIZE);
        file.write((char*) buffer, len);
    }

    /* write the users */
    for(User* user : users_) {
        int len = user->serialize(buffer, BUFFER_SIZE);
        file.write((char*) buffer, len);
    }

    /* close the file */
	file.close();

	return true;
}

void Database::addMsg(MessageServerText* msg) {
	backlog_.push_back(msg);
}

User* Database::addUser(const std::string& name, const std::string& pass) {
    /* make sure no user has already this name */
    for(User* user : users_)
        if(user->getName() == name)
            return nullptr;

    /* create the user */
	User* user = new User(name, pass);
	users_.push_back(user);

	return user;
}

void Database::print() const {
    cout << "The list of messages:" << endl;
    for(MessageServerText* text : backlog_)
        cout << text->getSize()    << "\t"
             << text->getAddress() << ":"
             << text->getPort()    << "\t"
             << text->getTime()    << "\t"
             << text->getName()    << "\t"
             << text->getMessage() << endl;
    cout << endl;

    cout << "The list of users:" << endl;
    for(User* user : users_)
        cout << user->getName() << endl;
}

User* Database::getUser(const string& name) const {
    /* search for the user */
    for(User* user : users_)
        if(user->getName() == name)
            return user;

    /* the user hasn't been found */
    return nullptr;
}

string Database::getFile() const {
	return file_;
}

std::vector<MessageServerText*> Database::getBacklog() const {
    return backlog_;
}

void Database::setFile(const string& file) {
	file_ = file;
}

bool Database::addSerializableObject(SerializableObject* obj) {
    User* user = nullptr;
    MessageServerText* text = nullptr;
    
    switch(obj->getID()) {
        case MSG_AUTH:
            user = static_cast<User*>(obj);
            users_.push_back(user);
            return true;
        case MSG_SERVER_TEXT:
            text = static_cast<MessageServerText*>(obj);
            backlog_.push_back(text);
            return true;
        default:
            return false;
    }
}

#ifdef __LINUX__
bool fileExists(const string& path) {
	/* allocate memory for the structure */
	struct stat buffer;

	/* test if the file exists */
	return (stat(path.c_str(), &buffer) == 0);
}
#endif
#ifdef __WIN32__
bool fileExists(const string& path) {
	return PathFileExists(path.c_str());
}
#endif
