#include "partitioner.h"
#include <omp.h>
#include <stdlib.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

int main(int argc, char **argv) {
  namespace po = boost::program_options;
  std::string index_file, data_type, gp_file, freq_file;
  std::string custom_graph_file, custom_meta_file;
  unsigned block_size, ldg_times, lock_nums, thead_nums, cut, ep_size;
  bool use_disk, visual, use_custom_graph;
  // Custom graph parameters
  uint32_t custom_node_num = 0;
  uint32_t custom_max_nbr_len = 0;
  uint32_t custom_max_alpha_range_len = 0;
  uint32_t custom_page_size = 8192;

  po::options_description desc{"Arguments"};
  try {
    desc.add_options()("help,h", "print information");
    desc.add_options()("data_type", po::value<std::string>(&data_type)->required(),
                       "data type <int8/uint8/float>");
    desc.add_options()("index_file", po::value<std::string>(&index_file)->default_value(""),
                       "diskann diskann index or mem index");
    desc.add_options()("gp_file", po::value<std::string>(&gp_file)->required(), "output gp file");
    desc.add_options()("freq_file", po::value<std::string>(&freq_file)->default_value(""), "freq_file[optional]");
    desc.add_options()("thread_nums,T", po::value<unsigned>(&thead_nums)->default_value(omp_get_num_procs()),
                       "threads_nums");
    desc.add_options()("lock_nums", po::value<unsigned>(&lock_nums)->default_value(0),
                       "lock node nums, the lock nodes will not participate in the follow LDG paritioning");
    desc.add_options()("block_size,B", po::value<unsigned>(&block_size)->default_value(1),
                       "block size for one partition, 1 for 4KB, 2 for 8KB and so on.");
    desc.add_options()("ldg_times,L", po::value<unsigned>(&ldg_times)->default_value(4),
                       "exec ldg partition alg times, usually 8 is enough.");
    desc.add_options()("use_disk", po::value<bool>(&use_disk)->default_value(1),
                       "Use 1 for use disk index (default), 0 for DiskANN mem index");
    desc.add_options()("visual", po::value<bool>(&visual)->default_value(0),
                       "see real time progress of graph partition");
    desc.add_options()("cut", po::value<unsigned>(&cut)->default_value(INF),
                       "cut adj list, use 3 means graph degree will be cut to 3");
    // Custom graph format options
    desc.add_options()("meta_file", po::value<std::string>(&custom_meta_file)->default_value(""),                                        
                   "custom meta file path (required when custom_graph=true)"); 
    desc.add_options()("custom_graph", po::value<bool>(&use_custom_graph)->default_value(false),
                       "use custom 3-file separated graph format (no vectors)");
    desc.add_options()("graph_file", po::value<std::string>(&custom_graph_file)->default_value(""),
                       "custom graph file path (required when custom_graph=true)");
    desc.add_options()("ep_size", po::value<unsigned>(&ep_size)->default_value(1),
                       "entry point size (for custom graph format)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      std::cout << desc;
      return 0;
    }
    po::notify(vm);
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
  }
  omp_set_num_threads(thead_nums);

  if (use_custom_graph) {
    // Use custom 3-file separated graph format                                                                                          
    if (custom_graph_file.empty() || custom_meta_file.empty()) {                                                                         
      std::cerr << "Error: --graph_file and --meta_file are required when custom_graph=true" << std::endl;   
      return -1;
    }

    GP::graph_partitioner partitioner;
    partitioner.load_custom_topology_graph(custom_graph_file.c_str(),
                                           custom_meta_file.c_str(),
                                           ep_size);

    if (!freq_file.empty()) {
      std::cout << "Warning: freq_file is not yet supported for custom graph format" << std::endl;
    }

    partitioner.graph_partition(gp_file.c_str(), ldg_times, lock_nums);
  } else {
    // Use original DiskANN format
    GP::graph_partitioner partitioner(index_file.c_str(), data_type.c_str(),
                                     use_disk, block_size, visual,
                                     freq_file, cut);
    partitioner.graph_partition(gp_file.c_str(), ldg_times, lock_nums);
  }

  return 0;
}
