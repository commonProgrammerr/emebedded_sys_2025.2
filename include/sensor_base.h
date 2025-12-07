#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H
typedef enum {
    SENSOR_OK = 0,
    SENSOR_ERROR = -1,
    SENSOR_NOT_FOUND = -2,
    SENSOR_TIMEOUT = -3
} SensorStatus_t;


struct sensor_base
{
    SensorStatus_t (*init)(struct sensor_base* self);
    SensorStatus_t (*read_data)(struct sensor_base* self, void* data);
    SensorStatus_t (*deinit)(struct sensor_base* self);
    void* context;
};

typedef struct sensor_base sensor_base_t;

typedef SensorStatus_t (*sensor_init_t)(sensor_base_t* self);
typedef SensorStatus_t (*sensor_read_data_t)(sensor_base_t* self, void* data);
typedef SensorStatus_t (*sensor_deinit_t)(sensor_base_t* self);


 

#endif // SENSOR_BASE_H