nebula_begin_module(physicsfeature)
fips_deps(physics application)
fips_ide_group(features)
fips_include_directories(${CODE_ROOT}/foundation)
fips_files(
		physicsfeatureunit.h
		physicsfeatureunit.cc
	)
	fips_dir(managers)
		fips_files(
			physicsmanager.h
			physicsmanager.cc
		)
	fips_dir(components)
		nebula_add_nidl(
			physics.nidl
		)
nebula_end_module()