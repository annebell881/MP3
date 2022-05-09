/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <thread>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
/*using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
//add the cordinator into the  info
/*using csce438::SNSCoordinator;
using csce438::Req;
using csce438::Rep;
using csce438::ClusterInfo;
using csce438::ServerInfo;
using csce438::JoinReq;
using csce438::FollowerInfo;*/
//There is an easier way
using namespace csce438;


struct Client {
  int username;
  bool connected = true;
  int following_file_size = 0;
  //change the Client to id's since they dont have a username
  std::vector<int> client_followers;
  std::vector<int> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//info for the coordinator
std::string c_hostname;
std::string c_port;

//create stubs
std::unique_ptr<csce438::SNSCoordinator::Stub> c_stub; //Coordinator
//the stub needs to be completed for slave/master proccess to connect them
std::unique_ptr<csce438::SNSService::Stub> s_stub = nullptr;

//server data
int s_id;
bool master; //is it the master server?
std::string s_port;
std::string s_hostname;

//Slave info
std::string slave_port;
std::string slave_addr;

//Vector that stores every client that has been created
std::vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(int user){
  int index = 0;
  for(Client c : client_db){
    if(c.username == user)
      return index;
    index++;
  }
  return -1;
}

//Need a heartbeat function to test if the master is alive
void heartBeatFunc(){
  Status stat;
  for(;;){
    HrtBeat hb;
    HrtBeat hb2;
    hb.set_sid(s_id);
    hb.set_stype(master);
    ClientContext cont;
    //set the heartbeat to the stub
    stat = c_stub->getServerCon(&cont,hb, &hb2);
    //check if their is a heartbeat
    if(!stat.ok()){
      std::cout << "Failed to detect Heartbeat" << std::endl;
      return;
    } 
    //else check every 10 seconds for heartbeats
    sleep(10);
  }
}

//getting a time stamp; https://stackoverflow.com/questions/6012663/get-unix-timestamp-with-c
std::string getTimeStamp(time_t t){
    char buf[32];
    struct tm* tm = localtime(&t);
    strftime (buf, 32, "%Y-%m-%d %H:%M:%S", tm);
    std::string outt = buf;

    return outt;
}

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    /*Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }*/
    //fix so that it works for the clusters
    int id = request->username();
    ClientContext cont;
    AllUsers user_base;
    Filler fill;
    c_stub->GetAllUsers(&cont, fill, &user_base);
    for (auto u : user_base.users())
    {
      list_reply->add_all_users(u);
    }
    std::ifstream ifs(std::to_string(id)+"followedBy.txt");
    std::string u = "";
    while (ifs.good()){
      std::getline(ifs, u, ',');
      if (u == "" || u == " "){
          continue;
      }
      list_reply->add_followers(atoi(u.c_str()));
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    int username1 = request->username();
    int username2 = request->arguments(0);
    int join_index = find_user(username2);
    if(join_index < 0 || username1 == username2)
      reply->set_msg("unknown user name");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	      reply->set_msg("you have already joined");
        return Status::OK;
      }
        user1->client_following.push_back(username2);
        user2->client_followers.push_back(username1);
        reply->set_msg("Follow Successful");
      

      //update the slave once our status is okay
      if (s_stub != nullptr){
        FollowData follow;
        follow.set_id(username1);
        follow.mutable_following()->Add(user1->client_following.begin(), user1->client_following.end());
        follow.mutable_followers()->Add(user1->client_followers.begin(), user1->client_followers.end());
        ClientContext context;
        Filler fill1;
        s_stub->FollowUpdate(&context, follow, &fill1);
      }
    }
      return Status::OK;  
  
  }

  /*Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("unknown follower username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("you are not follower");
        return Status::OK;
      }
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      reply->set_msg("UnFollow Successful");
    }
    return Status::OK;
  }*/
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    Client c;
    int username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	      reply->set_msg(msg);
        user->connected = true;
      }
    }
    if (s_stub != nullptr){
        ClientContext cc;
        s_stub->LoginUpdate(&cc, *request, reply);
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    Client *c;
    while(stream->Read(&message)) {
      int username = message.username();
      int user_index = find_user(username);
      c = &client_db[user_index];
 
      //Write the current message to "username.txt"
      /*std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      //google::protobuf::Timestamp temptime = message.timestamp();
      //std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      //std::string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream")
        user_file << fileinput;
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
	for(int i = 0; i<newest_twenty.size(); i++){
	  new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected)
	  temp_client->stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
        temp_client->following_file_size++;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
    }*/
    //If the client disconnected from Chat Mode, set connected to false
    
            // Write the current message to "usernametimeline.txt"
            std::string filename = std::to_string(username) + "timeline.txt";
            std::ofstream user_file(filename, std::ios::app | std::ios::out | std::ios::in);
            time_t temptime = message.timestamp();
            std::string ttime = getTimeStamp(temptime);
            std::string fileinput = ttime + " :: " + std::to_string(message.username()) + ":" + message.msg() + "\n";
            //"Set Stream" is the default message from the client to initialize the stream
            if (message.msg() != "Set Stream")
                user_file << fileinput;
            // If message = "Set Stream", print the first 20 chats from the people you follow
            
            // Send the message to each follower's stream
            std::vector<int>::const_iterator it;
            for (it = c->client_followers.begin(); it != c->client_followers.end(); it++)
            {
                int u_index = find_user(*it);
                if (u_index < 0 || u_index > client_db.size()){
                    std::cerr << "User " << *it << " not in client_db" << std::endl;
                    continue;
                }
                Client *temp_client = &client_db[u_index];
                if (temp_client->stream != 0 && temp_client->connected)
                    temp_client->stream->Write(message);
                
            }
    c->connected = false;
    }
    return Status::OK;
    
  }

   Status FollowUpdate(ServerContext *context, const FollowData *request) override
    {
        int id = request->id();
        int user_i = find_user(id);
        std::vector<int> followers;
        auto foll = request->followers();
        std::vector<int> following;
        auto foling = request->following();

        for (int i : foll) {
            followers.push_back(i);
        }

        for (int i : foling) {
            followers.push_back(i);
        }

        client_db[user_i].client_followers.clear();
        client_db[user_i].client_following.clear();

        for (int s : followers) client_db[user_i].client_followers.push_back(s);
        for (int s : following) client_db[user_i].client_following.push_back(s);

        return Status::OK;
    }

    Status TimelineUpdate(ServerContext *context, const Message *request) override
    {
        std::vector<std::string> messages;
        auto mesg = request->msg();

        std::copy(mesg.begin(), mesg.end(), messages.begin());

        std::ofstream ofs(std::to_string(request->username()), std::ios::trunc);

        for(std::string s : messages){
            ofs << s << std::endl;
        }

        return Status::OK;
    }

    Status LoginUpdate(ServerContext *context, const Request *request, Reply *reply) override
    {
      //stolen from the login of MP2 :)
      Client c;
      int username = request->username();
      int user_index = find_user(username);
      if (user_index < 0)
      {
          c.username = username;
          client_db.push_back(c);
          reply->set_msg("Login Successful!");
          user_index = find_user(username);
      }
      else
      {
          Client *user = &client_db[user_index];
          if (user->connected)
              reply->set_msg("Invalid Username");
          else
          {
              std::string msg = "Welcome Back " + user->username;
              reply->set_msg(msg);
              user->connected = true;
          }
      }
        return Status::OK;
    }

    Status SlaveToMaster(ServerContext *context, const ServerInfo *request, ServerInfo *response) override
    {
      slave_port = request->port();
      slave_addr = request->addre();
      std::string s_login_info = slave_addr + ":" + slave_port;
      s_stub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(s_login_info, grpc::InsecureChannelCredentials())));

      return Status::OK;
    }

  }
};


void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

      //Coordinator Stub
    std::string c_login_info = c_hostname + ":" + c_port;
    c_stub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(c_login_info, grpc::InsecureChannelCredentials())));

    //WE need the information for the cluster
    ClientContext cont;
    Status stat;
    ClusterInfo ci;
    ci.set_addr(s_hostname); 
    ci.set_port(port_no);
    ci.set_followid(s_id);
    ci.set_sid(master);
    ServerInfo si;
    //see if we can pull the cluster from the information
    stat = c_stub->ClusterSpace(&cont, ci, &si);
    if (!stat.ok()) {
        std::cerr << "Error, failed to create a  cluster." << std::endl;
    }
    //if the cluster works, then use the heartbeat 
    std::thread t(heartBeatFunc);
    t.detach();

    if (si.addre() != ""){
        //SNSService stub
        std::string s_login_info = si.addre() + ":" + si.port();
        s_stub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(s_login_info, grpc::InsecureChannelCredentials())));

        ClientContext context1;
        ServerInfo sib_id;
        sib_id.set_port(s_port);
        sib_id.set_addre("127.0.0.1"); 
        ServerInfo r_id;
        s_stub->SlaveToMaster(&context1, sib_id, &r_id);
    }
  server->Wait();
}

int main(int argc, char** argv) {
  //check if proper amount of aurguments
  if (argc < 9)
  {
    std::cerr << "Invalid number of arguments " <<  std::endl;
    exit(1);
  }
  //est the coordinator and the server information
  s_port = "3010";
  c_hostname = "127.0.0.1";
  c_port = "";
  s_id = -1;
  master = false;

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
            c_hostname = argv[i + 1];
            i++;
        }
        else if (arg == "-cp")
        {
            c_port = argv[i + 1];
            if (c_port.size() > 6)
            {
              std::cerr << "Invalid arguments " <<  std::endl;
              exit(1);
            }
            i++;
        }
        else if (arg == "-p")
        {
            s_port = argv[i + 1];
            if (s_port.size() > 6)
            {
              std::cerr << "Invalid arguments " <<  std::endl;
              exit(1);
            }
            i++;
        }
        else if (arg == "-id")
        {
            s_id = atoi(argv[i + 1]);
            if (s_id < 0)
            {
              std::cerr << "Invalid arguments " <<  std::endl;
              exit(1);
            }
            i++;
        }
        else if (arg == "-t")
        {
            std::string op(argv[i + 1]);
            if (op == "master")
                master = true;
            else if (op == "slave")
                master = false;
            else
            {
              std::cerr << "Invalid arguments " <<  std::endl;
              exit(1);
            }  
            i++;
        }
    }
    
  RunServer(s_port);

  return 0;
}

