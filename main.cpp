#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <atomic>
#include <future>
#include <random>

#include <sstream>
#include <string>
#include <fstream>

static const size_t fs_alignment = 4096;
static const size_t chunk_size = fs_alignment * 25; // 100K
static int n_files;  // Will be set from file list
static int reads_per_file;  // Will be set from command line
static size_t max_file_size;  // Will be set from command line
//static const bool with_lseek = true;                                                                                                                                                                                                                                                                                                                                      
static const int offset = fs_alignment*10;//1024*100;                                                                                                                                                                                                                                                                                                                               

void thread_read_lseek(std::atomic<int>& index, const size_t end, void* read_buffer, const std::vector<int>& fds) {

  int currentValue;
  while ((currentValue = index.fetch_add(1)) < end)
    {
        int fd = fds[currentValue];

        if (lseek(fd, offset, SEEK_SET) == -1)
          throw std::runtime_error ("lseek failed");

        size_t bytes_read = 0;
        while (bytes_read < chunk_size)
        {
            const size_t MAX_READ = 4096 * 1000;
            size_t to_request = (chunk_size - bytes_read > MAX_READ) ? MAX_READ : (chunk_size - bytes_read);
            ssize_t res = read (fd, read_buffer, to_request);

            if (res < 0)
                throw std::runtime_error (
                    "read:: read returned: " + std::to_string (res)
                    + " " + std::string (strerror (errno)) + " requested "
                    + std::to_string (to_request) + " offset " + std::to_string(offset) + " bytes_read " + std::to_string(bytes_read));
            if (to_request != (size_t)res)
              std::cerr << "requested " << to_request << " got " << res << " " << currentValue <<  std::endl;
            else
                bytes_read += res;
        }
    }
}

void thread_read(std::atomic<int>& index, const size_t end, void* read_buffer, const std::vector<int>& fds) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, max_file_size - chunk_size);

    int currentValue;
    while ((currentValue = index.fetch_add(1)) < end)
    {
        int fd = fds[currentValue];
        
        for (int read = 0; read < reads_per_file; read++) {
            size_t random_offset = (dis(gen) / fs_alignment) * fs_alignment;  // Align to page size
            
            size_t bytes_read = 0;
            while (bytes_read < chunk_size)
            {
                const size_t MAX_READ = 4096 * 1000;
                size_t to_request = (chunk_size - bytes_read > MAX_READ) ? MAX_READ : (chunk_size - bytes_read);
                ssize_t res = pread(fd, read_buffer, to_request, random_offset + bytes_read);
                if (res < 0)
                    throw std::runtime_error(
                        "read_posix:: pread returned: " + std::to_string(res)
                        + " " + std::string(strerror(errno)) + " requested "
                        + std::to_string(to_request) + " offset " + std::to_string(random_offset) + " bytes_read " + std::to_string(bytes_read));
                if (to_request != (size_t)res)
                    std::cerr << "requested " << to_request << " got " << res << " " << currentValue << std::endl;
                else
                    bytes_read += res;
            }
        }
    }
}

void thread_open(std::atomic<int>& index, const size_t end, std::vector<int>& fds, const std::vector<std::string>& paths)
{
    int i;
    while ((i = index.fetch_add(1)) < end)
    {
      fds[i] = open(paths[i].c_str(), O_DIRECT | O_RDONLY);
      if (fds[i] < 0)
        throw std::runtime_error("Failed to open file: " + paths[i]);
    }
}

std::vector<std::string> get_paths(std::string path)
{
    std::ifstream infile(path);
    std::vector<std::string> paths;
    std::string line;
    while (std::getline(infile, line))
    {
      std::istringstream iss(line);
      paths.push_back(line);
    }
    n_files = paths.size();  // Set n_files from actual file count
    return paths;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cout << "Please specify arguments: num_threads, path to file paths, file_size_in_bytes, reads_per_file" << std::endl;
        return -1;
    }

    size_t num_threads = std::atol(argv[1]);
    max_file_size = std::atol(argv[3]);
    reads_per_file = std::atol(argv[4]);

    std::vector<int> fds;
    std::vector<std::string> paths = get_paths(argv[2]);
    fds.resize(n_files);  // Use n_files from get_paths

    std::cout << "File size: " << max_file_size / (1024*1024) << " MB" << std::endl;
    std::cout << "First file: " << paths[0] << std::endl;
    std::cout << "Number of files: " << n_files << std::endl;
    std::cout << "Reads per file: " << reads_per_file << std::endl;
    std::atomic<int> currentIndex(0);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_threads; ++i)
      futures.push_back(std::async(std::launch::async, thread_open, std::ref(currentIndex), fds.size(), std::ref(fds), std::ref(paths)));

    for (auto& future : futures)
        future.get();
    futures.clear();
    std::cout << "Opened " << fds.size() << " files" << std::endl;

    std::vector<void*> buffers;
    for (int i = 0; i < num_threads; ++i) {
        void* read_buffer = aligned_alloc(fs_alignment, chunk_size);
        if (!read_buffer)
            throw std::runtime_error("Failed to alloc memory");
        buffers.emplace_back(read_buffer);
    }
    std::cout << "Memory allocated" << std::endl;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    for (int i = 0; i < 10; ++i) {
        currentIndex = 0;
        for (int i = 0; i < num_threads; ++i)
            futures.push_back(std::async(std::launch::async, thread_read, std::ref(currentIndex), fds.size(), buffers[i], std::ref(fds)));

        for (auto& future : futures)
            future.get();
        futures.clear();
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    // Calculate total bytes read per iteration
    size_t bytes_per_iteration = (size_t)n_files * chunk_size * reads_per_file;
    // Total bytes across all iterations
    size_t total_bytes = bytes_per_iteration * 10;
    // Calculate bandwidth in GB/s, accounting for parallel reads
    double bandwidth_gbps = (bytes_per_iteration * 8.0) / (duration_ms / 1000.0) / (1024 * 1024 * 1024);

    std::cout << "Params: " << num_threads << " threads, " << reads_per_file << " reads per file" << std::endl;
    std::cout << "Total bytes read: " << total_bytes / (1024*1024*1024) << " GB" << std::endl;
    std::cout << "Time: " << duration_ms << " ms" << std::endl;
    std::cout << "Bandwidth: " << bandwidth_gbps << " GB/s" << std::endl;
    std::cout << "IOPS: " << (n_files * reads_per_file * 10.0) / (duration_ms / 1000.0) << " operations/second" << std::endl;

    for (auto fd : fds)
        close(fd);

    for (auto buffer : buffers)
        free(buffer);


    return 0;
}
