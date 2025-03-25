# Random Read Benchmark

This benchmark measures random read performance from multiple files using direct I/O (O_DIRECT).

## Building

```bash
g++ -O3 -pthread main.cpp -o readfile_bench
```

## Usage

### 1. Generate Test Files

First, create a set of test files using the `create_files.sh` script:

```bash
./create_files.sh <directory> <num_files> <file_size_in_bytes>
```

Parameters:
- `directory`: Where to create the test files
- `num_files`: Number of files to create
- `file_size_in_bytes`: Size of each file in bytes

Example:
```bash
./create_files.sh /mnt/nvme1/hs-benchmark/data 1000 104857600
```
This will:
- Create 1000 files of 100MB (104857600 bytes) each in the specified directory
- Generate a file_list.txt in the current directory containing the paths to all created files

### 2. Run the Benchmark

```bash
./readfile_bench <num_threads> <path_to_file_list> <file_size_in_bytes> <reads_per_file>
```

Parameters:
- `num_threads`: Number of parallel threads to use for reading
- `path_to_file_list`: Path to a text file containing one file path per line (typically file_list.txt in the current directory)
- `file_size_in_bytes`: Size of the files to use for random offset generation
- `reads_per_file`: Number of random reads to perform per file

Example:
```bash
./readfile_bench 64 file_list.txt 104857600 10
```
This will:
- Use 64 threads
- Read from files listed in file_list.txt
- Generate random offsets up to 10MB (104857600 bytes)
- Perform 10 random reads per file

## Example Output

```bash
~# ./readfile_bench 64 file_list.txt 1024000 10
File size: 0 MB
First file: /mnt/nvme1/benchmarks/readfile_bench/data/file_0
Number of files: 100000
Reads per file: 10
Opened 100000 files
Memory allocated
Params: 64 threads, 10 reads per file
Total bytes read: 953 GB
Time: 104343 ms
Bandwidth: 7.31184 GB/s
IOPS: 95837.8 operations/second
```
