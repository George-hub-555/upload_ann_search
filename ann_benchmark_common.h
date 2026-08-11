#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace ann_bench {

// fvecs/ivecs 的每条记录都是“int32 维度 + 维度个元素”。模板参数决定
// 元素按 float（fvecs）还是 int32（ivecs）读取，不能把整个文件直接裸读。
template <typename T>
struct Vecs {
    size_t rows = 0;
    size_t columns = 0;
    std::vector<T> values;
};

template <typename T>
Vecs<T> read_vecs(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open " + path.string());
    }

    Vecs<T> result;
    int32_t columns = 0;
    while (input.read(reinterpret_cast<char*>(&columns), sizeof(columns))) {
        if (columns <= 0) {
            throw std::runtime_error("invalid vector dimension in " + path.string());
        }
        if (result.columns == 0) {
            result.columns = static_cast<size_t>(columns);
        } else if (result.columns != static_cast<size_t>(columns)) {
            throw std::runtime_error("inconsistent vector dimension in " + path.string());
        }
        const size_t old_size = result.values.size();
        result.values.resize(old_size + result.columns);
        input.read(
                reinterpret_cast<char*>(result.values.data() + old_size),
                static_cast<std::streamsize>(result.columns * sizeof(T)));
        if (!input) {
            throw std::runtime_error("truncated vector file " + path.string());
        }
        ++result.rows;
    }
    if (result.rows == 0) {
        throw std::runtime_error("empty vector file " + path.string());
    }
    return result;
}

template <typename T>
std::vector<T> parse_list(const std::string& text) {
    std::vector<T> values;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (item.empty()) {
            throw std::invalid_argument("empty value in list: " + text);
        }
        std::stringstream converter(item);
        T value{};
        converter >> value;
        if (!converter || !converter.eof()) {
            throw std::invalid_argument("invalid value in list: " + item);
        }
        values.push_back(value);
    }
    if (values.empty()) {
        throw std::invalid_argument("list must not be empty");
    }
    return values;
}

inline double median(std::vector<double> values) {
    if (values.empty()) {
        throw std::invalid_argument("cannot compute median of an empty list");
    }
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    if ((values.size() % 2) != 0) {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) / 2.0;
}

// Recall@k 使用集合交集定义：每条查询返回的 top-k 与 ground truth top-k
// 相交多少个，最后除以 query_count * k。距离值本身不参与 Recall 计算。
inline double recall_at_k(
        const std::vector<size_t>& labels,
        const Vecs<int32_t>& ground_truth,
        size_t query_count,
        int top_k) {
    size_t matches = 0;
    for (size_t query = 0; query < query_count; ++query) {
        std::unordered_set<size_t> returned;
        returned.reserve(static_cast<size_t>(top_k) * 2);
        for (int rank = 0; rank < top_k; ++rank) {
            const size_t label = labels[query * static_cast<size_t>(top_k) + rank];
            if (label != std::numeric_limits<size_t>::max()) {
                returned.insert(label);
            }
        }
        for (int rank = 0; rank < top_k; ++rank) {
            const int32_t truth =
                    ground_truth.values[query * ground_truth.columns + static_cast<size_t>(rank)];
            if (truth >= 0 && returned.count(static_cast<size_t>(truth)) != 0) {
                ++matches;
            }
        }
    }
    return static_cast<double>(matches) /
            static_cast<double>(query_count * static_cast<size_t>(top_k));
}

inline void validate_dataset(
        const Vecs<float>& base,
        const Vecs<float>& query,
        const Vecs<int32_t>& ground_truth,
        const std::vector<int>& top_k_values,
        size_t query_count) {
    if (base.columns != query.columns) {
        throw std::runtime_error("base/query dimensions differ");
    }
    if (ground_truth.rows < query_count) {
        throw std::runtime_error("ground truth contains fewer rows than selected queries");
    }
    for (const int top_k : top_k_values) {
        if (top_k <= 0 || static_cast<size_t>(top_k) > ground_truth.columns) {
            throw std::runtime_error("top-k is invalid or exceeds ground-truth width");
        }
    }
}

inline void write_header(std::ofstream& output) {
    output
            << "algorithm,implementation,top_k,threads,search_parameter,search_value,"
            << "recall_at_k,qps,repeat_count,warmup_queries,base_count,query_count,dimension,"
            << "index_parameters,status,note\n";
}

}  // namespace ann_bench
