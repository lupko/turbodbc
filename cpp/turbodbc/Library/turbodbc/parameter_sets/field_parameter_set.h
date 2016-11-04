#pragma once

#include <cpp_odbc/statement.h>
#include <turbodbc/parameter_sets/bound_parameter_set.h>
#include <memory>
#include <vector>

namespace turbodbc {

class field_parameter_set {
public:
	field_parameter_set(std::shared_ptr<cpp_odbc::statement const> statement,
	                    std::size_t parameter_sets_to_buffer);

	void add_parameter_set(std::vector<nullable_field> const & parameter_set);

	void flush();

	long get_row_count();

	~field_parameter_set();

private:
	void check_parameter_set(std::vector<nullable_field> const & parameter_set) const;
	void add_parameter(std::size_t index, nullable_field const & value);
	void recover_unwritten_parameters_below(std::size_t parameter_index, std::size_t last_active_row);
	void rebind_parameter_to_hold_value(std::size_t index, field const & value);

	std::shared_ptr<cpp_odbc::statement const> statement_;
	std::size_t parameter_sets_to_buffer_;
	bound_parameter_set parameters_;
	std::size_t current_parameter_set_;
};

}
