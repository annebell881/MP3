#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <map>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <stdexcept>
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
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using namespace csce438;


void RunServer(std::string port_no)
{
    std::string server_address = "0.0.0.0:" + port_no;
    SNSCoordImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Coordinator listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char **argv)
{

    if (argc != 3)
    {
        std::cerr << "Bad arguments"<< std::endl;
        exit(1);
    }

    std::string port = "3011";

    for (int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);

        if (argc == i + 1)
        {
            std::cerr << "Bad arguments"<< std::endl;
            exit(1);
        }
        else if (arg == "-p")
        {
            port = argv[i + 1];
            if (port.size() > 6){
                std::cerr << "Bad arguments"<< std::endl;
                exit(1);
            }
            i++;
        }
    }

    // Run RPC server shit
    RunServer(port);

    return 0;
}