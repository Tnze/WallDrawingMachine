/**
 * @file coordinate.hpp
 *
 * @author Tnze <cjd001113@outlook.com>
 */
#include <cmath>
#include "tasker.hpp"

class PositionStatus
{
    void xy_to_mn(float sx, float sy, int &sm, int &sn)
    {
        sm = (int)(sqrtf(powf(-R - sx, 2) + sy * sy) / sl);
        sn = (int)(sqrtf(powf(R - sx, 2) + sy * sy) / sl);
    }

    void mn_to_xy(int sm, int sn, float &sx, float &sy)
    {
        float fm = sm * sl;
        float fn = sn * sl;
        sx = (fm * fm - fn * fn) / (4 * R);
        // sy = sqrtf((fn - sx + R) * (fn + sx - R)); // or
        sy = sqrtf((fm - sx - R) * (fm + sx + R));
    }

public:
    float sl;
    float R;
    int m, n;
    PositionStatus(float step_len, float D, int m, int n) : sl(step_len), R(D / 2), m(m), n(n) {}

    void Pos(float &x, float &y, float &M, float &N)
    {
        mn_to_xy(m, n, x, y);
        M = m * sl;
        N = n * sl;
    }

    void MoveTo(float x, float y, int &dm, int &dn)
    {
        dm = m;
        dn = n;
        xy_to_mn(x, y, m, n);
        dm = m - dm;
        dn = n - dn;
    }
};

// TODO: not implemented
class LinerMotorTask : public Task
{
public:
    bool direction;
    Stepper *stepper;
    int total_steps, walked_steps;
    float x0, y0, dx, dy, sl;

    LinerMotorTask(Stepper &stepper, unsigned long start_time, unsigned long duration, int steps, float pos, float step_length, float x0, float y0, float x1, float y1)
        : Task(start_time, duration),
          stepper(&stepper),
          direction(steps > 0),
          total_steps(steps > 0 ? steps : -steps),
          walked_steps(0), sl(step_length),
          x0(x0 + pos), y0(y0), dx(x1 - x0), dy(y1 - y0) {}
    unsigned long run(unsigned long now) override
    {
        // not start yet
        if (now < start_time)
            return start_time - now;
        // has been end
        if (walked_steps >= total_steps || total_steps == 0)
            return TASK_STEP_RESULT_STOP;

        // calculate velocity
        float t = (float)walked_steps / (float)total_steps; // between 0 and 1
        float x = x0 + t * dx;
        float y = y0 + t * dy;
        float dt = sl * sqrtf(x * x + y * y) / (dx * x + dy * y);

        // printf("[m %s]", direction ? "->" : "<-");
        stepper->step(direction ? 1 : -1);
        walked_steps++;

        return abs(dt * 1000);
    }
};