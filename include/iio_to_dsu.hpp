#pragma once

#include "iio.hpp"

#include <array>
#include <cstdint>
#include <string>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
class IIOToDSU
{
  private:
    std::array<uint8_t, 3> m_var_matrix;
    std::array<bool, 3> m_polarity_matrix;
    static constexpr uint8_t cm_not_set = 0xFF;

    void FixBroken(IIOToDSU* default_matrix)
    {
        if (default_matrix == nullptr)
        {
            std::cout << "Error: default_matrix is nullptr in FixBroken"
                      << std::endl;
            return;
        }

        for (int i = 0; i < 3; i++)
        {
            m_var_matrix[i] = default_matrix->m_var_matrix[i];
            m_polarity_matrix[i] = default_matrix->m_polarity_matrix[i];
        }
    }

  public:
    IIOToDSU(std::string matrix, IIOToDSU* default_matrix)
        : m_var_matrix{cm_not_set, cm_not_set, cm_not_set},
          m_polarity_matrix{true, true, true} // default is positive
    {
        constexpr std::string_view remove = " \t\n\r";
        matrix.erase(0, matrix.find_first_not_of(remove));
        matrix.erase(matrix.find_last_not_of(remove) + 1);

        std::cout << "Processing matrix: " << matrix << std::endl;

        if (matrix.empty())
        {
            if (default_matrix != nullptr)
            {
                FixBroken(default_matrix);
            } else
            {
                throw std::runtime_error(
                    "Matrix string is empty and no default matrix provided");
            }
            return;
        }

        size_t pos = 0;
        for (int i = 0; i < 3; i++)
        {
            size_t end = matrix.find(',', pos);
            if (end == std::string::npos)
            {
                end = matrix.length();
            }

            std::string value = matrix.substr(pos, end - pos);

            m_polarity_matrix[i] = true;

            for (char c : value)
            {
                if (c == '+')
                {
                    m_polarity_matrix[i] = true;
                } else if (c == '-')
                {
                    m_polarity_matrix[i] = false;
                } else if (c == 'x' || c == 'y' || c == 'z')
                {
                    m_var_matrix[i] = c - 'x';
                } else if (c == 'X' || c == 'Y' || c == 'Z')
                {
                    m_var_matrix[i] = c - 'X';
                } else
                {
                    if (default_matrix == nullptr)
                    {
                        throw std::runtime_error(
                            "ERROR, FATAL: Invalid character in matrix string");
                        // uhhh you should stop here but just in case...
                        return;
                    }

                    std::cout << "ERROR, NOT FATAL: Invalid character in matrix "
                                 "string: '"
                              << c << "'" << std::endl;

                    FixBroken(default_matrix);
                    return;
                }
            }

            pos = end + 1;
        }

        if (m_var_matrix[0] == cm_not_set || m_var_matrix[1] == cm_not_set ||
            m_var_matrix[2] == cm_not_set)
        {
            if (default_matrix != nullptr)
            {
                std::cout
                    << "WARNING: Matrix not fully initialized, using default"
                    << std::endl;
                FixBroken(default_matrix);
                return;
            }
            throw std::runtime_error(
                "ERROR, FATAL: Matrix not fully initialized, missing values: " +
                std::to_string(m_var_matrix[0]) + ", " +
                std::to_string(m_var_matrix[1]) + ", " +
                std::to_string(m_var_matrix[2]));
        }

        std::cout << "Parsed matrix:" << std::endl;
        for (int i = 0; i < 3; i++)
        {
            std::cout << "out[" << i
                      << "] = " << (m_polarity_matrix[i] ? "+" : "-")
                      << "xyz"[m_var_matrix[i]] << "\n";
        }
    }

    auto Convert(const iio::Vec3& input) -> iio::Vec3
    {
        iio::Vec3 output(0, 0, 0);

        for (int i = 0; i < 3; i++)
        {
            if (m_var_matrix[i] != cm_not_set)
            {
                output[i] =
                    input[m_var_matrix[i]] * (m_polarity_matrix[i] ? 1.0 : -1.0);
            }
        }

        return output;
    }
};
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
