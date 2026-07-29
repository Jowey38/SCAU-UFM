add_executable(scau_preproc main.cpp)
target_link_libraries(scau_preproc
    PRIVATE
        scau::stcf
        scau::warnings
)
target_compile_features(scau_preproc PRIVATE cxx_std_20)
