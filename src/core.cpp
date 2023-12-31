#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "core.h"
#include "xcl/xcl.h"

// #include <wayland-client.h>
// #include "spdlog/async.h"
// #include "spdlog/sinks/stdout_color_sinks.h"
#include "qst.pb.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#if defined(_WIN32) || defined(_WIN64)
#include <shlobj.h>
#include <windows.h>

#include <cuchar>

#elif defined(__linux__)
#endif
namespace qst {

  QstBackendCore::QstBackendCore(int argc, char *argv[])
    : addr()
    , server()
    , searcher()
    // , logger(
    // spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>("backend")
    // )
    , xcl() {
    // spdlog::set_default_logger(logger);
    if(std::setlocale(LC_ALL, "en_US.UTF-8") == nullptr) {
      spdlog::error("QstBackendCore\t: setlocale failed");
      std::exit(1);
    }
    spdlog::set_level(spdlog::level::trace);
    spdlog::debug("QstBackendCore\t: start");
    if(argc < 2) {
      showHelp();
    }
    for(int i = 1; i < argc; ++i) {
      if(std::strcmp(argv[i], "--addr") == 0) {
        this->addr = argv[++i];
        spdlog::debug("Set address to listen on: {}", addr);
      } else if(std::strcmp(argv[i], "--help") == 0) {
        showHelp();
      }
    }
    if(this->addr.empty()) {
      spdlog::error("QstBackendCore\t: no addr");
      std::exit(1);
    }
#if defined(_WIN32) || defined(_WIN64)
    LPWSTR homePath = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &homePath);
    if(SUCCEEDED(hr)) {
      xcl.load(std::wstring(homePath) + L"\\.config\\qst\\config.xcl");
      CoTaskMemFree(homePath);
    }
#elif defined(__linux__)
    auto homePath = std::getenv("HOME");
    if(homePath != nullptr) {
      spdlog::error("QstBackendCore\t: no HOME");
      std::exit(1);
      xcl.load(std::string(homePath) + "/.config/qst/config.xcl");
    }
#endif
    xcl.try_insert("run_count");
    searcher.init();
  }
  QstBackendCore::~QstBackendCore() {
    server->Shutdown();
    xcl.save();
  }

  void QstBackendCore::exec() {
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server = builder.BuildAndStart();
    spdlog::debug("Start server");
    server->Wait();
  }
  ::grpc::Status QstBackendCore::ListApp(
    ::grpc::ServerContext *context, const ::qst_comm::Input *request, ::qst_comm::DisplayList *response) {
    spdlog::debug("ListApp\t: input\t= {}", request->str());
    ::qst_comm::Display display;
    this->last_result = searcher.search(request->str());
    for(auto info : this->last_result) {
      if(!(info->is_config)) {
        auto run_count = xcl.find<long>(std::string("run_count'") + info->name);
        if(run_count) {
          info->run_count = *run_count;
        }
        spdlog::debug("ListApp\t: load run_count\t= {}", info->run_count);
        info->is_config = true;
      }
    }
    std::sort(this->last_result.begin(), this->last_result.end(),
      [](AppInfo *a, AppInfo *b) { return a->run_count > b->run_count; });
    for(auto info : this->last_result) {
      spdlog::debug("ListApp\t: name\t= {}", info->name, info->run_count);
      spdlog::debug("ListApp\t: exec\t= {}", info->exec);
      spdlog::debug("ListApp\t: argHint\t= {}", info->args_hint);
      spdlog::debug("ListApp\t: run_count\t= {}", info->run_count);
      display.set_name(std::string(info->name));
      display.set_arghint(info->args_hint);
      response->add_list()->CopyFrom(display);
    }
    return ::grpc::Status::OK;
  }
  ::grpc::Status QstBackendCore::RunApp(
    ::grpc::ServerContext *context, const ::qst_comm::ExecHint *request, ::qst_comm::Empty *response) {
    AppInfo *info = this->last_result[request->idx()];
    spdlog::debug("RunApp\t: {}", info->exec);
    pm.new_process(info->exec, request->args());
    info->run_count++;
    xcl.insert_or_assign<long>(std::string("run_count'") + info->name, info->run_count);
    xcl.save(true);
    return ::grpc::Status::OK;
  }
  void QstBackendCore::showHelp() {
    std::cout << "Usage: qst [options]" << std::endl
              << "Options:" << std::endl
              << "  --addr <addr>       Set the address to listen on" << std::endl
              << "  --help              Show this help message" << std::endl;
    std::exit(0);
  }
}  // namespace qst