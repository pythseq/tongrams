#pragma once

#include <deque>

#include "../utils/util.hpp"

namespace tongrams
{
    template<typename Comparator,
             typename LineHandler>
    struct sorter
    {
        sorter(uint64_t n,
               Comparator& comparator,
               std::string const& output_filename,
               std::string const& tmp_dir)
            : m_n(n)
            , m_batch_size(0)
            , m_cur_filename(0)
            , m_comparator(comparator)
            , m_output_filename(output_filename)
            , m_tmp_dir(tmp_dir)
        {}

        ~sorter() {
            merge_batches();
        }

        template<typename Iterator>
        void sort(Iterator begin, Iterator end,
                  std::string const& output_filename)
        {
            if (begin == end) {
                throw std::runtime_error("nothing to sort");
            }

            size_t n = end - begin;
            m_batch_size = n;
            std::cout << "batch size = " << m_batch_size << std::endl;
            util::logger("sorting " + output_filename);
            std::sort(begin, end, m_comparator);
            util::logger("flushing " + output_filename);
            flush(begin, end, output_filename);
            m_files.push_back(output_filename);
        }

    private:
        uint64_t m_n;
        uint64_t m_batch_size;
        uint64_t m_cur_filename;
        Comparator& m_comparator;
        std::string m_output_filename;
        std::deque<std::string> m_files;
        std::string m_tmp_dir;

        std::string next_tmp_filename() const {
            return m_tmp_dir + "/." + m_output_filename +
                                "." + std::to_string(m_cur_filename);
        }

        void merge_batches()
        {
            assert(m_files.size());

            if (m_files.size() == 1) {
                auto fn = m_files.front();
                merge(fn, next_tmp_filename(),
                      m_output_filename);
                std::remove(fn.c_str());
            }

            while (m_files.size() > 1)
            {
                auto fn1 = m_files.front();
                m_files.pop_front();
                auto fn2 = m_files.front();
                m_files.pop_front();

                std::string output_filename(m_files.empty() ? m_output_filename
                                                            : next_tmp_filename());
                merge(fn1, fn2, output_filename);

                std::remove(fn1.c_str());
                std::remove(fn2.c_str());

                m_files.push_back(output_filename);
                ++m_cur_filename;
            }

            assert(m_files.size() == 1);
        }

        template<typename Iterator>
        void flush(Iterator begin, Iterator end,
                   std::string const& output_filename)
        {           
            std::ofstream os(output_filename.c_str());
            std::string line_to_write;
            uint64_t n = uint64_t(end - begin);

            if (LineHandler::value_t == value_type::count) {
                building_util::write(os, std::to_string(n) + "\n");
            }

            for (auto it = begin; it != end; ++it) {
                LineHandler::format_line(*it, line_to_write);
                building_util::write(os, line_to_write + "\n");
            }

            os.close();
        }

        void merge(std::string const& filename1,
                   std::string const& filename2,
                   std::string const& output_filename)
        {
            util::logger("merging files " + filename1 +
                         " and " + filename2 +
                         " into " + output_filename);

            std::ofstream os;
            os.open(output_filename.c_str(),
                    std::ofstream::ate
                  | std::ofstream::app);

            std::ifstream input1(filename1.c_str());
            std::ifstream input2(filename2.c_str());
            std::string line1, line2;

            if (LineHandler::value_t == value_type::count) {
                uint64_t num_grams = 0;
                if (!building_util::is_empty(input1)) {
                    std::getline(input1, line1);
                    num_grams += std::stoull(line1);
                }
                if (!building_util::is_empty(input2)) {
                    std::getline(input2, line2);
                    num_grams += std::stoull(line2);
                }
                if (num_grams) {
                    building_util::write(os, std::to_string(num_grams) += "\n");
                } else {
                    throw std::runtime_error("num of grams must be > 0");
                }
            }

            if (!building_util::is_empty(input1)
            &&  !building_util::is_empty(input2))
            {   
                std::getline(input1, line1);
                std::getline(input2, line2);
                auto t1 = LineHandler::parse_line(line1);
                auto t2 = LineHandler::parse_line(line2);

                while (true) {
                    if (m_comparator(t1, t2)) {
                        building_util::write(os, line1 += "\n");
                        if (std::getline(input1, line1)) {
                            t1 = LineHandler::parse_line(line1);
                        } else {
                            building_util::write(os, line2 += "\n");
                            break;
                        }
                    } else {
                        building_util::write(os, line2 += "\n");
                        if (std::getline(input2, line2)) {
                            t2 = LineHandler::parse_line(line2);
                        } else {
                            building_util::write(os, line1 += "\n");
                            break;
                        }
                    }
                }
            }

            if (input1.eof()) {
                while (std::getline(input2, line2)) {
                    building_util::write(os, line2 += "\n");
                }
            } else {
                while (std::getline(input1, line1)) {
                    building_util::write(os, line1 += "\n");
                }
            }

            os.close();
        }
    };
}
