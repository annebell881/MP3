#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"

//NOTE THAT THE MP2 SOULTION IS USED IN THE SOULTION
#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
//add the cordinator into the  info
using csce438::SNSCoordinator;
using csce438::Req;
using csce438::Rep;
using csce438::ClusterInfo;
using csce438::ServerInfo;
using csce438::JoinReq;
using csce438::FollowerInfo;
using csce438::HeartBeat;

//Client needs to retrieve the Coordinator Infomation
std::string coord_host;
std::string coord_port;

//Client is also going to need the following data
int client_ID;
//client ID is also frequently used throughout the code
std::unique_ptr<csce438::SNSCoord::Stub> client_stub;


Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
        //a big point is that the master might disconnect; this func is to check
        void renew_conn();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;

        IReply Login();
        IReply List();
        IReply Follow(const std::string& username2);
        IReply UnFollow(const std::string& username2);
        void Timeline(const std::string& username);


};

int main(int argc, char** argv) {
    //main needs to be updated to match the call
    if(argv != 5){
        std::cerr << "Invalid number of arguments " <<  std::endl;
        exit(1);
    }

    std::string hostname = "localhost";
    //std::string username = "default";
    //this MP uses client ID in place of username
    int client_ID = -1; //by default this is a -1 since we dont have any clients
    std::string port = "3010";

    //loop through and find the correct arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);

        if (argc == i + 1)
        {
            std::cerr << "Invalid arguments " <<  std::endl;
            exit(1);
        }

        if (arg == "-cip")
        {
            hostname = argv[i + 1];
            i++;
        }
        else if (arg == "-cp")
        {
            port = argv[i + 1];
            if (port.size() > 6){
                std::cerr << "Invalid number of arguments " <<  std::endl;
                exit(1);
            }
            i++;
        }
        else if (arg == "-id")
        {
            client_ID = atoi(argv[i + 1]);
            if (client_ID < 0)
            {
                std::cerr << "Invalid number of arguments " <<  std::endl;
                exit(1);
            }
            i++;
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    //need to add functions to connect to the cordinator
    std::string cord_login_info = coord_host + ":" + coord_port;
    client_stub = std::<SNSCoord::Stub>(SNSCoord::NewStub(grpc::CreateChannel(cord_login_info, grpc::InsecureChannelCredentials())));
    //grab the context to create the log in info below in MP2
    ClientContext contex;
    JoinReq join_rq;
    join_rq.set_id(client_ID);
    ClusterInfo clusI;
    client_stub->GetConnection(&contex, join_rq,clusI);
    //MP2 soultion
    //update it to match the cluster
    std::string login_info = clusI.addres() + ":" + clusI.port();
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
    hostname = clusI.addres();
    port = clusI.port();
    //thread the cluster into the coordinator
    std::thread clust_thread(&Client::renew_conn,this);
    t.detach();

    IReply ire = Login();
    if(!ire.grpc_status.ok()) {
        return -1;
    }
    return 1;
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// - JOIN/LEAVE and "<username>" are separated by one space.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Join" service method for JOIN command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Join(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
	// ------------------------------------------------------------
    IReply ire;
    std::size_t index = input.find_first_of(" ");
    if (index != std::string::npos) {
        std::string cmd = input.substr(0, index);


        /*
        if (input.length() == index + 1) {
            std::cout << "Invalid Input -- No Arguments Given\n";
        }
        */

        std::string argument = input.substr(index+1, (input.length()-index));

        if (cmd == "FOLLOW") {
            return Follow(argument);
        }
        //no use in the unfollow command 
        /*else if(cmd == "UNFOLLOW") {
            return UnFollow(argument);
        }*/
    } else {
        if (input == "LIST") {
            return List();
        } else if (input == "TIMELINE") {
            ire.comm_status = SUCCESS;
            return ire;
        }
    }

    ire.comm_status = FAILURE_INVALID;
    return ire;
}

void Client::processTimeline()
{
    Timeline(username);
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
}

IReply Client::List() {
    //Data being sent to the server
    Request request;
    request.set_username(username);

    //Container for the data from the server
    ListReply list_reply;

    //Context for the client
    ClientContext context;

    Status status = stub_->List(&context, request, &list_reply);
    IReply ire;
    ire.grpc_status = status;
    //Loop through list_reply.all_users and list_reply.following_users
    //Print out the name of each room
    if(status.ok()){
        ire.comm_status = SUCCESS;
        std::string all_users;
        std::string following_users;
        for(std::string s : list_reply.all_users()){
            ire.all_users.push_back(s);
        }
        for(std::string s : list_reply.followers()){
            ire.followers.push_back(s);
        }
    }
    return ire;
}
        
IReply Client::Follow(const std::string& username2) {
    Request request;
    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;
    ClientContext context;

    Status status = stub_->Follow(&context, request, &reply);
    IReply ire; ire.grpc_status = status;
    if (reply.msg() == "unkown user name") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "unknown follower username") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "you have already joined") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    } else if (reply.msg() == "Follow Successful") {
        ire.comm_status = SUCCESS;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }
    return ire;
}

//Function not needed in MP3
/*IReply Client::UnFollow(const std::string& username2) {
    Request request;

    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;

    ClientContext context;

    Status status = stub_->UnFollow(&context, request, &reply);
    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "unknown follower username") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "you are not follower") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "UnFollow Successful") {
        ire.comm_status = SUCCESS;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}*/

IReply Client::Login() {
    Request request;
    request.set_username(username);
    Reply reply;
    ClientContext context;

    Status status = stub_->Login(&context, request, &reply);

    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "you have already joined") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    } else {
        ire.comm_status = SUCCESS;
    }
    return ire;
}

void Client::Timeline(const std::string& username) {
    ClientContext context;

    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
            stub_->Timeline(&context));

    //Thread used to read chat messages and send them to the server
    std::thread writer([username, stream]() {
            std::string input = "Set Stream";
            Message m = MakeMessage(username, input);
            stream->Write(m);
            while (1) {
            input = getPostMessage();
            m = MakeMessage(username, input);
            stream->Write(m);
            }
            stream->WritesDone();
            });

    std::thread reader([username, stream]() {
            Message m;
            while(stream->Read(&m)){

            google::protobuf::Timestamp temptime = m.timestamp();
            std::time_t time = temptime.seconds();
            displayPostMessage(m.username(), m.msg(), time);
            }
            });

    //Wait for the threads to finish
    writer.join();
    reader.join();
}

void Client::renew_conn(){
    //check if the master died every 15 seconds maybe?
    for(;;){
        sleep(15);
        //try to reconnect to the client
        ClientContext cont;
        JoinReq join_req;
        join_req.set_id(client_ID);
        ClusterInfo clusI;
        client_stub->GetConnection(&cont, join_req, &clusI);
        //now we need to fix the ports and clients
        if (port != clusI.port() || hostname != clusI.addr()){
            std::string login_info = clusI.addr() + ":" + clusI.port();
            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
                grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

            hostname = clusI.addr();
            port = clusI.port();
        }
    }
    
}