#include <cstring>

#ifdef _WIN32
#define SCAU_BMI_EXPORT extern "C" __declspec(dllexport)
#else
#define SCAU_BMI_EXPORT extern "C"
#endif

namespace {

bool g_initialized = false;
double g_start_time = 100.0;
double g_current_time = 100.0;
double g_water_level[2] = {2.5, 3.5};
double g_lateral_discharge = 0.0;
const char* g_var_names[2] = {"water_level", "lateral_discharge"};

}  // namespace

SCAU_BMI_EXPORT int initialize(const char* config_file) {
    if (config_file == nullptr || config_file[0] == '\0') {
        return 11;
    }
    g_initialized = true;
    g_start_time = 100.0;
    g_current_time = 100.0;
    g_water_level[0] = 2.5;
    g_water_level[1] = 3.5;
    g_lateral_discharge = 0.0;
    return 0;
}

SCAU_BMI_EXPORT int update(double dt) {
    if (!g_initialized || dt <= 0.0) {
        return 22;
    }
    g_current_time += dt;
    g_water_level[0] += 0.1;
    g_water_level[1] += 0.2;
    return 0;
}

SCAU_BMI_EXPORT int finalize() {
    g_initialized = false;
    return 0;
}

SCAU_BMI_EXPORT void get_start_time(double* time) {
    *time = g_start_time;
}

SCAU_BMI_EXPORT void get_current_time(double* time) {
    *time = g_current_time;
}

SCAU_BMI_EXPORT void get_time_step(double* dt) {
    *dt = 30.0;
}

SCAU_BMI_EXPORT void get_var(const char* name, void** ptr) {
    if (std::strcmp(name, "water_level") == 0) {
        *ptr = g_water_level;
        return;
    }
    if (std::strcmp(name, "lateral_discharge") == 0) {
        *ptr = &g_lateral_discharge;
        return;
    }
    *ptr = nullptr;
}

SCAU_BMI_EXPORT void set_var(const char* name, const void* ptr) {
    if (std::strcmp(name, "lateral_discharge") == 0) {
        g_lateral_discharge = *static_cast<const double*>(ptr);
    }
}

SCAU_BMI_EXPORT void get_var_count(int* count) {
    *count = 2;
}

SCAU_BMI_EXPORT void get_var_name(int index, char* name) {
    if (index < 0 || index >= 2) {
        name[0] = '\0';
        return;
    }
    std::strcpy(name, g_var_names[index]);
}
