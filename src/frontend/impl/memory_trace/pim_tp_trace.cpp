#include <filesystem>
#include <fstream>
#include <iostream>

#include "frontend/frontend.h"
#include "base/exception.h"

namespace Ramulator {

namespace fs = std::filesystem;

class PimTPTrace : public IFrontEnd, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, PimTPTrace, "PimTPTrace", "PIM tensor product trace.")

  private:
    struct Trace {
      AddrVec_t addr_vec;
      int num_paths;
      int dim_h;
      int dim_e;
      int dim_out;
    };

    std::vector<Trace> m_trace;
    size_t m_trace_length = 0;
    size_t m_curr_trace_idx = 0;

    int m_default_dim_h = 0;
    int m_default_dim_e = 0;
    int m_default_dim_out = 0;

    Logger_t m_logger;

  public:
    void init() override {
      std::string trace_path_str = param<std::string>("path").desc("Path to the PIM tensor product trace file.").required();
      m_clock_ratio = param<uint>("clock_ratio").required();
      m_default_dim_h = param<int>("default_dim_h").desc("Default dim(l_h) when not provided in trace.").default_val(5);
      m_default_dim_e = param<int>("default_dim_e").desc("Default dim(l_e) when not provided in trace.").default_val(7);
      m_default_dim_out = param<int>("default_dim_out").desc("Default dim(l_out) when not provided in trace.").default_val(5);

      m_logger = Logging::create_logger("PimTPTrace");
      m_logger->info("Loading trace file {} ...", trace_path_str);
      init_trace(trace_path_str);
      m_logger->info("Loaded {} lines.", m_trace.size());
    };

    void tick() override {
      const Trace& t = m_trace[m_curr_trace_idx];
      Request req(t.addr_vec, Request::Type::PimTP);
      req.pim_num_paths = t.num_paths;
      req.pim_dim_h = t.dim_h;
      req.pim_dim_e = t.dim_e;
      req.pim_dim_out = t.dim_out;
      m_memory_system->send(req);

      m_curr_trace_idx = (m_curr_trace_idx + 1) % m_trace_length;
    };

  private:
    void init_trace(const std::string& file_path_str) {
      fs::path trace_path(file_path_str);
      if (!fs::exists(trace_path)) {
        throw ConfigurationError("Trace {} does not exist!", file_path_str);
      }

      std::ifstream trace_file(trace_path);
      if (!trace_file.is_open()) {
        throw ConfigurationError("Trace {} cannot be opened!", file_path_str);
      }

      std::string line;
      while (std::getline(trace_file, line)) {
        std::vector<std::string> tokens;
        tokenize(tokens, line, " ");

        if (tokens.size() != 3 && tokens.size() != 6) {
          throw ConfigurationError("Trace {} format invalid!", file_path_str);
        }

        if (tokens[0] != "TP") {
          throw ConfigurationError("Trace {} format invalid!", file_path_str);
        }

        std::vector<std::string> addr_vec_tokens;
        tokenize(addr_vec_tokens, tokens[1], ",");
        AddrVec_t addr_vec;
        for (const auto& token : addr_vec_tokens) {
          addr_vec.push_back(std::stoll(token));
        }

        int num_paths = std::stoi(tokens[2]);
        int dim_h = m_default_dim_h;
        int dim_e = m_default_dim_e;
        int dim_out = m_default_dim_out;
        if (tokens.size() == 6) {
          dim_h = std::stoi(tokens[3]);
          dim_e = std::stoi(tokens[4]);
          dim_out = std::stoi(tokens[5]);
        }

        m_trace.push_back({addr_vec, num_paths, dim_h, dim_e, dim_out});
      }

      trace_file.close();
      m_trace_length = m_trace.size();
    };

    bool is_finished() override {
      return true;
    };
};

}  // namespace Ramulator
