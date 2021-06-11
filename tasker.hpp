#include <Stepper.h>

constexpr auto TASK_STEP_RESULT_STOP = -1;

constexpr auto TASK_ID_SERVO = 0;
constexpr auto TASK_ID_MOTOR = 1;

class Task
{
protected:
  Task(unsigned long start_time, unsigned long duration)
      : start_time(start_time),
        duration(duration),
        _last_delay(0) {}

public:
  unsigned long _last_delay;
  unsigned long start_time;
  unsigned long duration;
  virtual unsigned long run(unsigned long now) = 0;
};

// class ServoTask : public Task
// {
// public:
//   int pos_from;
//   int pos_to;

//   ServoTask(int start_time, int duration, int from, int to)
//       : Task(start_time, duration),
//         pos_from(from),
//         pos_to(to) {}
//   int run(int now) override
//   {
//     if (now < start_time)
//       return start_time - now;
//     int pos = pos_from + (pos_to - pos_from) * (now - start_time) / duration;
//     // printf("[s%3d]", pos);
//     return now - start_time > duration ? TASK_STEP_RESULT_STOP
//                                        : TASK_STEP_RESULT_WAIT;
//   }
// };

class MotorTask : public Task
{
public:
  Stepper *stepper;
  bool direction;
  int left_steps;

  MotorTask(Stepper &stepper, unsigned long start_time, unsigned long duration, bool direction, int steps)
      : Task(start_time, duration),
        stepper(&stepper),
        direction(steps > 0),
        left_steps(steps > 0 ? steps : -steps)
  {
    int speed = abs(steps) * 1000 / duration;
    stepper.setSpeed(speed > 0 ? speed : 1);
  }
  unsigned long run(unsigned long now) override
  {
    if (now < start_time)
      return start_time - now;
    // printf("[m %s]", direction ? "->" : "<-");
    stepper->step(direction ? 1 : -1);
    return now - start_time >= duration
               ? TASK_STEP_RESULT_STOP
               : (duration - now + start_time) / (--left_steps);
  }
};

void run_tasks(Task **task_list, int task_count)
{
  unsigned long start_time = micros(), now = 0, *min_delay, dt, delay_min_i = 0;
  do
  {
    min_delay = &(task_list[delay_min_i]->_last_delay);
    if (micros() - start_time - now < *min_delay)
      continue;
    dt = *min_delay;
    now += dt;
    // Run the scatuled task and update delay table
    for (int i = 0; i < task_count; i++)
    {
      unsigned long *last_delay = &(task_list[i]->_last_delay);
      if (*last_delay == TASK_STEP_RESULT_STOP)
        continue;
      task_list[i]->_last_delay -= dt;
      if (*last_delay <= 0)
        *last_delay = task_list[i]->run(now);
    }
    // Search for next wake-up time
    for (int i = 0; i < task_count; i++)
    {
      unsigned long *last_delay = &(task_list[i]->_last_delay);
      if (*min_delay == TASK_STEP_RESULT_STOP ||
          (*last_delay != TASK_STEP_RESULT_STOP &&
           *last_delay <= *min_delay))
        delay_min_i = i;
    }
  } while (*min_delay != TASK_STEP_RESULT_STOP);
}
