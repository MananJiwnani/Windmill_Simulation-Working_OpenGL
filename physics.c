#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include "state.h"
#include "physics.h"

void update_physics(void)
{
    if (isRandom)
    {
        if (rand() % 1000 == 0)
        {
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        progstep += (0.008 * (wind_speed_target - progstep * 1000.0)) / 1000.0;
        wind_y += 0.002 * (wind_angle_target - wind_y);
        wind_y = fmod(wind_y, 360.0);
        if (wind_y < 0.0) wind_y += 360.0;
    }

    double acc = progstep * cos(wind_y / 180.0 * M_PI) * wind_acc_factor - wing_speed * turbine_factor;
    wing_speed += acc;
    wing_z += wing_speed;

    if (wing_z >= 360.0)
        wing_z -= 360.0;
    if (wing_z < 0.0)
        wing_z += 360.0;
}
