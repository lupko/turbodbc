#include <turbodbc_numpy/set_numpy_parameters.h>

#include <turbodbc_numpy/ndarrayobject.h>

#include <turbodbc/errors.h>
#include <turbodbc/make_description.h>
#include <turbodbc/type_code.h>
#include <turbodbc/time_helpers.h>

#include <iostream>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif
#include <sql.h>

namespace turbodbc_numpy {

namespace {

    struct parameter_converter {
        parameter_converter(pybind11::array const & data, pybind11::array_t<bool> const & mask) :
            data(data),
            mask(mask)
        {}

        virtual void initialize(turbodbc::bound_parameter_set & parameters, std::size_t parameter_index) = 0;
        virtual void set_batch(turbodbc::parameter & parameter, std::size_t start, std::size_t elements) = 0;

        virtual ~parameter_converter() = default;

        pybind11::array const & data;
        pybind11::array_t<bool> const & mask;
    };

    template <typename Value>
    struct binary_converter : public parameter_converter {
        binary_converter(pybind11::array const & data, pybind11::array_t<bool> const & mask, turbodbc::type_code type) :
            parameter_converter(data, mask),
            type(type)
        {}

        void initialize(turbodbc::bound_parameter_set & parameters, std::size_t parameter_index) final
        {
            parameters.rebind(parameter_index, turbodbc::make_description(type, 0));
        }

        void set_batch(turbodbc::parameter & parameter, std::size_t start, std::size_t elements) final
        {
            auto data_ptr = data.unchecked<Value, 1>().data(0);
            auto & buffer = parameter.get_buffer();
            std::memcpy(buffer.data_pointer(), data_ptr + start, elements * sizeof(Value));
            if (mask.size() != 1) {
                auto const indicator = buffer.indicator_pointer();
                auto const mask_start = mask.unchecked<1>().data(start);
                for (std::size_t i = 0; i != elements; ++i) {
                    indicator[i] = (mask_start[i] == NPY_TRUE) ? SQL_NULL_DATA : sizeof(Value);
                }
            } else {
                intptr_t const sql_mask = (*mask.data() == NPY_TRUE) ? SQL_NULL_DATA : sizeof(Value);
                std::fill_n(buffer.indicator_pointer(), elements, sql_mask);
            }
        }
    private:
        turbodbc::type_code type;
    };


    struct timestamp_converter : public parameter_converter {
        timestamp_converter(pybind11::array const & data, pybind11::array_t<bool> const & mask) :
            parameter_converter(data, mask)
        {}

        void initialize(turbodbc::bound_parameter_set & parameters, std::size_t parameter_index) final
        {
            parameters.rebind(parameter_index, turbodbc::make_description(turbodbc::type_code::timestamp, 0));
        }

        void set_batch(turbodbc::parameter & parameter, std::size_t start, std::size_t elements) final
        {
            auto & buffer = parameter.get_buffer();
            auto const data_start = data.unchecked<std::int64_t, 1>().data(start);

            bool const uses_mask = (mask.size() != 1);
            if (uses_mask) {
                auto const mask_start = mask.unchecked<1>().data(start);
                for (std::size_t i = 0; i != elements; ++i) {
                    auto element = buffer[i];
                    if (mask_start[i] == NPY_TRUE) {
                        element.indicator = SQL_NULL_DATA;
                    } else {
                        turbodbc::microseconds_to_timestamp(data_start[i], element.data_pointer);
                        element.indicator = sizeof(SQL_TIMESTAMP_STRUCT);
                    }
                }
            } else {
                if (*mask.data() == NPY_TRUE) {
                    std::fill_n(buffer.indicator_pointer(), elements, static_cast<std::int64_t>(SQL_NULL_DATA));
                } else {
                    for (std::size_t i = 0; i != elements; ++i) {
                        auto element = buffer[i];
                        turbodbc::microseconds_to_timestamp(data_start[i], element.data_pointer);
                        element.indicator = sizeof(SQL_TIMESTAMP_STRUCT);
                    }
                }
            }
        }
    };

    struct date_converter : public parameter_converter {
        date_converter(pybind11::array const & data, pybind11::array_t<bool> const & mask) :
            parameter_converter(data, mask)
        {}

        void initialize(turbodbc::bound_parameter_set & parameters, std::size_t parameter_index) final
        {
            parameters.rebind(parameter_index, turbodbc::make_description(turbodbc::type_code::date, 0));
        }

        void set_batch(turbodbc::parameter & parameter, std::size_t start, std::size_t elements) final
        {
            auto & buffer = parameter.get_buffer();
            auto const data_start = data.unchecked<std::int64_t, 1>().data(start);

            bool const uses_mask = (mask.size() != 1);
            if (uses_mask) {
                auto const mask_start = mask.unchecked<1>().data(start);
                for (std::size_t i = 0; i != elements; ++i) {
                    auto element = buffer[i];
                    if (mask_start[i] == NPY_TRUE) {
                        element.indicator = SQL_NULL_DATA;
                    } else {
                        turbodbc::days_to_date(data_start[i], element.data_pointer);
                        element.indicator = sizeof(SQL_DATE_STRUCT);
                    }
                }
            } else {
                if (*mask.data() == NPY_TRUE) {
                    std::fill_n(buffer.indicator_pointer(), elements, static_cast<std::int64_t>(SQL_NULL_DATA));
                } else {
                    for (std::size_t i = 0; i != elements; ++i) {
                        auto element = buffer[i];
                        turbodbc::days_to_date(data_start[i], element.data_pointer);
                        element.indicator = sizeof(SQL_DATE_STRUCT);
                    }
                }
            }
        }
    };



    std::vector<std::unique_ptr<parameter_converter>> make_converters(std::vector<std::tuple<pybind11::array, pybind11::array_t<bool>, std::string>> const & columns)
    {
        std::vector<std::unique_ptr<parameter_converter>> converters;

        for (std::size_t i = 0; i != columns.size(); ++i) {
            auto const & data = std::get<0>(columns[i]);
            auto const & mask = std::get<1>(columns[i]);
            auto const & dtype = std::get<2>(columns[i]);
            if (dtype == "int64") {
               converters.emplace_back(new binary_converter<std::int64_t>(data, mask, turbodbc::type_code::integer));
            } else if (dtype == "float64") {
                converters.emplace_back(new binary_converter<double>(data, mask, turbodbc::type_code::floating_point));
            } else if (dtype == "datetime64[us]") {
                converters.emplace_back(new timestamp_converter(data, mask));
            } else if (dtype == "datetime64[D]") {
                converters.emplace_back(new date_converter(data, mask));
            } else {
                std::ostringstream message;
                message << "Unsupported NumPy dtype for column " << (i + 1) << " of " << columns.size();
                message << " (unsupported type: " << dtype << ")";
                throw turbodbc::interface_error(message.str());
            }
        }

        return converters;
    }

}

void set_numpy_parameters(turbodbc::bound_parameter_set & parameters, std::vector<std::tuple<pybind11::array, pybind11::array_t<bool>, std::string>> const & columns)
{
    if (parameters.number_of_parameters() != columns.size()) {
        throw turbodbc::interface_error("Number of passed columns is not equal to the number of parameters");
    }

    if (columns.size() == 0) {
        return;
    }

    auto converters = make_converters(columns);
    for (std::size_t i = 0; i != columns.size(); ++i) {
        converters[i]->initialize(parameters, i);
    }

    auto const total_sets = std::get<0>(columns.front()).size();

    for (std::size_t start = 0; start < total_sets; start += parameters.buffered_sets()) {
        auto const in_this_batch = std::min(parameters.buffered_sets(), total_sets - start);
        for (std::size_t i = 0; i != columns.size(); ++i) {
            converters[i]->set_batch(*parameters.get_parameters()[i], start, in_this_batch);
        }
        parameters.execute_batch(in_this_batch);
    }
}

}