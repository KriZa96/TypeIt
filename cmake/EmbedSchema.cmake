# Schema SQL, compiled into the binary (TECHNICAL section 4.2).
#
# Not read from disk at runtime: a migration that depends on a file being
# installed correctly is a migration that fails on exactly the machines where
# recovering is hardest. The binary carries what it needs to build its own
# database.
#
# Generated rather than hand-maintained so that adding `002_whatever.sql` is
# the whole of adding a migration — nothing to remember to register.

function(typeit_embed_schema target)
    set(schema_dir "${CMAKE_CURRENT_SOURCE_DIR}/schema")
    file(GLOB schema_files RELATIVE "${schema_dir}" "${schema_dir}/*.sql")
    list(SORT schema_files)

    set(entries "")
    foreach(schema_file IN LISTS schema_files)
        if(NOT schema_file MATCHES "^([0-9]+)_.*\\.sql$")
            message(FATAL_ERROR "schema/${schema_file} must be named NNN_description.sql")
        endif()
        set(version "${CMAKE_MATCH_1}")
        # Leading zeros would be read as octal in C++.
        math(EXPR version "${version} + 0")

        file(READ "${schema_dir}/${schema_file}" sql)
        if(sql MATCHES "\\)SQL\"")
            message(FATAL_ERROR "schema/${schema_file} contains the raw-string terminator")
        endif()
        string(APPEND entries
               "    Migration{.version = ${version},\n"
               "              .name = \"${schema_file}\",\n"
               "              .sql = R\"SQL(\n${sql})SQL\",\n"
               "              .stamp = \"PRAGMA user_version = ${version}\"},\n")

        # Re-run CMake when a schema file changes, or a stale generated file
        # silently ships the previous schema.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${schema_dir}/${schema_file}")
    endforeach()

    if(entries STREQUAL "")
        message(FATAL_ERROR "no schema files found in ${schema_dir}")
    endif()

    set(TYPEIT_SCHEMA_ENTRIES "${entries}")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/Schema.generated.cpp.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/generated/Schema.generated.cpp" @ONLY)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/Schema.generated.cpp")
endfunction()
