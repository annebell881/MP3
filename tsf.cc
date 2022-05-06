#include <ctime>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <thread>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <sys/types.h>
#include <sys/stat.h>
#include <iterator>
#include <string>
#include <stack>
#include <set>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Duration;
using google::protobuf::Timestamp;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;

using namespace csce438;

// Coord info
std::string c_hostname;
std::string c_port;

// Meta Follower info
int foll_id;
int s_id;
std::set<int> client_ids;
std::string hostname;

//Stub
std::unique_ptr<csce438::SNSCoord::Stub> c_stub;

void f_update(int);

class SNSFollowerImpl final : public SNSFollower::Service{
    Status Following(ServerContext *context, const FollowPair* request, Reply* response) override {
        //much like the following in the other servers
        int id = request->id();
        int fid = request->fid();
        //print out if they are following, if not then return the status of canclled
        if(client_ids.count(fid)){
            std::ofstream fol_s (std::to_string(fid) + "followedBy.txt", std::ios::app | std::ios::out | std::ios::in);
            fol_s << id << ",";
            fol_s.close();
            std::cout << fid << " is followed by " << id << std::endl;
        }
        else{
            std::cerr << "Client with ID '" << fid << "' is not managed by this follower, ID: '" << foll_id << "'\n";
            return Status::CANCELLED;
        }
        return Status::OK;
    }
    //if it is a new client we need to connect them to the server
    Status newClient(ServerContext *context, const JoinReq* request, Reply* response) override {
        int id = request->id();
        //request the id and attach it to the server
        if(client_ids.insert(id).second){
            std::thread t1(f_update, id);
            t1.detach();
            return Status::OK;
        }
        //if the id is taken then it should not attach to the server 
        else {
            std::cout << "The new client " << id << " already existed" << std::endl;
            return Status::CANCELLED;
        }
        
    }

    Status newMessage(ServerContext *context, const Message* request, Reply* response) override {
        int recv_id = request->id();

        if (!client_ids.count(recv_id)) {
            std::cerr << "Message recipiant " << recv_id << " is not managed by this follower" << std::endl;
            return Status::CANCELLED;
        }
        //send the messages out to the followers
        std::ofstream ofs(std::to_string(recv_id) + "fTimelines.txt", std::ios::app | std::ios::out | std::ios::in);
        for (auto msg : request->msgs()){
            ofs << msg << std::endl;
        }
        ofs.close();
        return Status::OK;
    }
};

//common timestamp update
std::string getTimeStamp(time_t t){
    char buf[32];
    struct tm* tm = localtime(&t);
    strftime (buf, 32, "%Y-%m-%d %H:%M:%S", tm);
    std::string outt = buf;

    return outt;
}

//Check if user_id follows anyone and then tell their follower about it
void f_update(int user_id) {
    std::set<int> following;
    std::ifstream ifs(std::to_string(user_id)+"follows.txt");
    //then we need to create the stream for the user
    if (ifs.fail()){
        std::cout << user_id << " doesn't yet have a 'follows.txt'. Creating one..." << std::endl;
            std::ofstream ofs(std::to_string(user_id)+"follows.txt");
            ofs.close();
    }

    std::string u = "";
    while(ifs.good()){
        std::getline(ifs, u, ',');
        if (u == "") continue;
        //continue to print out the stream continuously
        following.insert(atoi(u.c_str()));
    }
    ifs.close();

    for(;;){
        sleep(25);
        struct stat statbuf;
        if(stat((std::to_string(user_id)+"follows.txt").c_str(), &statbuf)!=0){
            std::cerr << "Stat failed to find follows info for " << user_id << std::endl;
            perror("stat");
            continue;
        }
        //we look into the time since the last edit for the followers
        time_t last_write = statbuf.st_mtim.tv_sec;
        time_t now = time(nullptr);
        double difft = std::difftime(now, last_write);
        std::cout << difft << " seconds have passed since " << std::to_string(user_id)+"follows.txt was last edited" << std::endl;

        if (difft < 30){ //if there's been less than 30 seconds since last edit
            std::cout << "A follower update was detected for " << user_id << std::endl;
            //after the dection we need to update the stream of followers
            std::set<int> fol;
            std::ifstream ifs(std::to_string(user_id)+"follows.txt");
            u = "";
            while(ifs.good()){
                std::getline(ifs, u, ',');
                if (u == "") continue;
                fol.insert(atoi(u.c_str()));
            }
            if (fol.empty()){
                std::cout << "No follows found for " << user_id << std::endl;
                continue;
            }
            std::set<int> diff;
            std::set_difference (fol.begin(), fol.end(), following.begin(), following.end(), std::inserter(diff, diff.end()));
            
            if (diff.empty()){
                std::cerr << "No new follows were found for " << user_id << std::endl;
                continue;
            }

            //Update the following set
            following.clear();
            std::copy(fol.begin(), fol.end(), std::inserter(following, following.end()));

            //update the notice so that the followedBy.txt is updated as the followr
            for (int i : diff){
                if (client_ids.count(i)){ 
                    std::ofstream fol_s (std::to_string(i) + "followedBy.txt", std::ios::app | std::ios::out | std::ios::in);
                    fol_s << std::to_string(user_id) << ",";
                    fol_s.close();
                }
                else {
                    JoinReq jr;
                    FollowerInfo ci;
                    ClientContext context;

                    jr.set_id(i);

                    Status status = c_stub->GetFollowing(&context, jr, &ci);
                    if (!status.ok()){
                        std::cerr << "The coordinator did not return info for " << i << "'s synchronizer" << std::endl;
                        continue;
                    }
                

                    //RPC the follower to send the update
                    std::string f_login_info = ci.addr() + ":" + ci.port();
                    auto f_stub = std::unique_ptr<SNSFollow::Stub>(SNSFollow::NewStub(grpc::CreateChannel(f_login_info, grpc::InsecureChannelCredentials())));

                    FollowPair fp;
                    Reply r;
                    ClientContext context1;

                    fp.set_id(user_id); //user_id follows i
                    fp.set_fid(i);

                    status = f_stub->Following(&context1, fp, &b);
                }
            }
        }
    }
}

void RunServer(std::string port_no){
    std::string server_address = "0.0.0.0:" + port_no;
    SNSFollowerImpl service;

    //Coordinator Stub
    std::string c_login_info = c_hostname + ":" + c_port;
    c_stub = std::unique_ptr<SNSCoord::Stub>(SNSCoord::NewStub(grpc::CreateChannel(c_login_info, grpc::InsecureChannelCredentials())));

    //Spawn Follower for Coordinator
    ClientContext context;
    FollowerInfo fi;
    fi.set_addr(hostname); 
    fi.set_port(port_no);
    fi.set_id(foll_id);
    fi.set_sid(s_id);
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    
    server->Wait();
}

int main(int argc, char **argv)
{
    if (argc < 7)
    {
        printUsage();
    }

    std::string port = "3011";

    c_hostname = "127.0.0.1";
    c_port = "";
    foll_id = -1;

    for (int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);

        if (argc == i + 1)
        {
            std::cerr << "Bad argument: " << arg << std::endl;
            exit(1);
        }

        if (arg == "-cip")
        {
            c_hostname = argv[i + 1];
            i++;
        }
        else if (arg == "-cp")
        {
            c_port = argv[i + 1];
            if (c_port.size() > 6)
                std::cerr << "Bad argument: " << arg << std::endl;
                exit(1);
            i++;
        }
        else if (arg == "-p")
        {
            port = argv[i + 1];
            if (port.size() > 6){
                std::cerr << "Bad argument: " << arg << std::endl;
                exit(1)
            }

            i++;
        }
        else if (arg == "-id")
        {
            foll_id = atoi(argv[i + 1]);
            if (foll_id < 0)
            {
                std::cerr << "Bad argument: " << arg << std::endl;
                exit(1);
            }
            i++;
        }
    }

    if(foll_id < 0) {
        std::cerr << "Bad argument: " << arg << std::endl;
    }
    else{
        s_id = foll_id;
    }

    char hostbuff[32];
    int host = gethostname(hostbuff, 32);
    struct hostent *host_entry = gethostbyname(hostbuff);
    char* IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
    hostname = IP;

    RunServer(port);

    return 0;
}
