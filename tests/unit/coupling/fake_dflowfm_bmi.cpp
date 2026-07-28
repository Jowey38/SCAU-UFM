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
double g_vol1[3] = {10.0, 20.0, 30.0};
const char* g_var_names[3] = {"water_level", "lateral_discharge", "vol1"};

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
    g_vol1[0] = 10.0;
    g_vol1[1] = 20.0;
    g_vol1[2] = 30.0;
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
    if (std::strcmp(name, "vol1") == 0) {
        *ptr = g_vol1;
        return;
    }
    *ptr = nullptr;
}

SCAU_BMI_EXPORT void set_var(const char* name, const void* ptr) {
    if (std::strcmp(name, "lateral_discharge") == 0) {
        g_lateral_discharge = *static_cast<const double*>(ptr);
    }
}

SCAU_BMI_EXPORT void get_var_type(const char* name, char* type) {
    if (std::strcmp(name, "water_level") == 0
        || std::strcmp(name, "lateral_discharge") == 0
        || std::strcmp(name, "vol1") == 0) {
        std::strcpy(type, "double");
        return;
    }
    type[0] = '\0';
}

SCAU_BMI_EXPORT void get_var_rank(const char* name, int* rank) {
    if (std::strcmp(name, "water_level") == 0 || std::strcmp(name, "vol1") == 0) {
        *rank = 1;
        return;
    }
    if (std::strcmp(name, "lateral_discharge") == 0) {
        *rank = 0;
        return;
    }
    *rank = -1;
}

SCAU_BMI_EXPORT void get_var_shape(const char* name, int* shape) {
    for (int i = 0; i < 6; ++i) {
        shape[i] = 0;
    }
    if (std::strcmp(name, "water_level") == 0) {
        shape[0] = 2;
    } else if (std::strcmp(name, "vol1") == 0) {
        shape[0] = 3;
    }
}

SCAU_BMI_EXPORT void get_var_count(int* count) {
    *count = 3;
}

SCAU_BMI_EXPORT void get_var_name(int index, char* name) {
    if (index < 0 || index >= 3) {
        name[0] = '\0';
        return;
    }
    std::strcpy(name, g_var_names[index]);
}
