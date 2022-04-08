# Copyright (c) 2022 Chanjung Kim (paxbun). All rights reserved.

function (target_use_cpp_standard TARGET_NAME STANDARD)
	set_target_properties(${TARGET_NAME} PROPERTIES
		CXX_STANDARD ${STANDARD}
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
endfunction()
