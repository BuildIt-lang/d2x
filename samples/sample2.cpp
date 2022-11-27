#include "d2x/d2x.h"
#include <iostream>

#define STR1(x)  #x
#define STR(x)  STR1(x)
#define BASE_DIR STR(BASE_DIR_X)

static builder::dyn_var<char*(int*)> to_str(builder::as_global("std::to_string"));	

int main(int argc, char* argv[]) {

	std::cout << "#include <stdio.h>\n";
	std::cout << "#include \"d2x_runtime/d2x_runtime.h\"\n";

	d2x::d2x_context context;
	
	std::cout << context.begin_section();
	
	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 1, "main", 0});
	std::cout << "int main(int argc, char* argv[]) {" << std::endl;
	context.nextl();


	d2x::runtime_value_resolver r1 ([&](auto v) -> auto {
		//return "Value of " + v;
		builder::dyn_var<int*> addr = d2x::rt::find_stack_var(v);
		return "Value of " + v + " = " + to_str(addr[0]);
	});

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 2, "main", 1});
	context.create_var("v1");
	context.update_var("v1", r1);
	context.set_var_here("v1", r1);
	std::cout << "\tint v1 = 21;" << std::endl;
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 3, "main", 2});
	std::cout << "\tprintf(\"Hello %d\\n\", v1);" << std::endl;	
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 4, "main", 3});
	std::cout << "\treturn 0;" << std::endl;
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 4, "main", 3});
	std::cout << "}" << std::endl;
	context.nextl();		

	context.emit_function_info(std::cout);
	context.end_section();

	return 0;
}
