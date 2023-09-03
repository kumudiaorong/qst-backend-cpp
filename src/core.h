#ifndef QST_BACKEND_CORE_H
#define QST_BACKEND_CORE_H
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>

#include "appinfo.h"
#include "qst.grpc.pb.h"
// #include "spdlog/logger.h"
#include "trie.hpp"

namespace qst {
 
  class QstBackendCore : public qst_comm::Interact::Service {
  public:
    std::string addr;
    std::unique_ptr<grpc::Server> server;
    AppSearcher searcher;
    // std::shared_ptr<spdlog::logger> logger;

    QstBackendCore(int argc, char *argv[]);
    void exec();
  protected:
    ::grpc::Status ListApp(
      ::grpc::ServerContext *context, const ::qst_comm::Input *request, ::qst_comm::DisplayList *response) override;
    ::grpc::Status RunApp(
      ::grpc::ServerContext *context, const ::qst_comm::ExecHint *request, ::qst_comm::Empty *response) override;
    void process(std::string args, bool stdio = false);
    void showHelp();
  };
}  // namespace qst
#endif