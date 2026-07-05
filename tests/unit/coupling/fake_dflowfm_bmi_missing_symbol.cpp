#ifdef _WIN32
#define SCAU_BMI_EXPORT extern "C" __declspec(dllexport)
#else
#define SCAU_BMI_EXPORT extern "C"
#endif

SCAU_BMI_EXPORT int initialize(const char* config_file) {
    return config_file != nullptr && config_file[0] != '\0' ? 0 : 11;
}

SCAU_BMI_EXPORT int update(double dt) {
    return dt > 0.0 ? 0 : 22;
}

SCAU_BMI_EXPORT int finalize() {
    return 0;
}

SCAU_BMI_EXPORT void get_start_time(double* time) {
    *time = 0.0;
}

SCAU_BMI_EXPORT void get_current_time(double* time) {
    *time = 0.0;
}

SCAU_BMI_EXPORT void get_var(const char*, void** ptr) {
    *ptr = nullptr;
}

SCAU_BMI_EXPORT void set_var(const char*, const void*) {}

SCAU_BMI_EXPORT void get_var_count(int* count) {
    *count = 0;
}
