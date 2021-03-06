#include <Stepper.h>
#include <stdint.h>
#include "coordinate.hpp"
#include "gcode.hpp"
#include "tasker.hpp"
#include "servo.hpp"
#include <Preferences.h>

#define BLUETOOTH "Wall Drawing Machine"

#ifdef BLUETOOTH
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;
#endif

double_t speed, speed_fast;
Stream *Host;
PositionStatus position_status(35.0f * PI / 2048.0f, 300, 5587, 5587);
int32_t last_line_code = 0; // 串口行号
Stepper stepperR(64, 32, 33, 25, 26);
Stepper stepperL(64, 19, 18, 5, 17);
Preferences prefs;

void load_settings()
{
    prefs.begin("wall_drawing_machine");
    if (prefs.getBytesLength("position_status") == sizeof(PositionStatus))
        prefs.getBytes("position_status", (void *)&position_status, sizeof(PositionStatus));
    speed = prefs.getDouble("speed", 200);
    speed_fast = prefs.getDouble("speed_fast", 400);
    prefs.end();
}

void save_settings()
{
    prefs.begin("wall_drawing_machine");
    prefs.putBytes("position_status", (void *)&position_status, sizeof(PositionStatus));
    prefs.putDouble("speed", speed);
    prefs.putDouble("speed_fast", speed_fast);
    prefs.end();
}

void setup()
{
#ifdef BLUETOOTH
    SerialBT.begin(BLUETOOTH);
#endif
    Serial.begin(115200);
    init_servo();
    stepperR.setSpeed(1000);
    stepperL.setSpeed(1000);
}

void exec_gcode(String line)
{
    Host->println("// received: " + line);
    String args;
    if (line.length() < 1)
        return;
    int fs = line.indexOf(' ');
    int num = line.substring(1, fs).toInt();

    float x = 0, y = 0;
    float M, N;
    int m = 0, n = 0;

    switch (line[0])
    {
    case 'G':
    {
        float x0, y0;
        position_status.Pos(x0, y0, M, N);
        bool fast_move = false;
        switch (num)
        {
        case 0: // G0 快速移动
            fast_move = true;
        case 1: // G1 直线插补
            args = line.substring(fs + 1);

            for (;;) // 读取目标位置
            {
                char c = args[0];
                fs = args.indexOf(' ');
                String num = args.substring(1, fs == -1 ? args.length() : fs);
                switch (c)
                {
                case 'X':
                    x = num.toFloat();
                    break;
                case 'Y':
                    y = num.toFloat();
                    break;
                case 'm':
                    m = num.toInt();
                    break;
                case 'n':
                    n = num.toInt();
                    break;
                }
                if (fs == -1)
                    break;
                args = args.substring(fs + 1);
            }
            if (m == 0 && n == 0)
                position_status.MoveTo(x, y, m, n);
            else
                fast_move = false;
            Host->printf("// move steps: (m%d, n%d)\n", m, n);

            if (m || n)
            { // Move (m, n)
                unsigned long duration = sqrtl(m * m + n * n) * 1000 / (fast_move ? speed_fast : speed);

                MotorTask left_task(stepperL, 0, duration, true, m);
                MotorTask right_task(stepperR, 0, duration, true, n);
                Task *task_list[2] = {&left_task, &right_task};

                Task::run_tasks(task_list, 2);
            }

            break;

        case 2: // G1 移动舵机
            args = line.substring(fs + 1);
            fs = args.indexOf(' ');
            String num = args.substring(1, fs == -1 ? args.length() : fs);
            if (args[0] == 'S')
            {
                // String num = args.substring(1, fs == -1 ? args.length() : fs);
                // ServoTask servo_task(0, 1000, 500, 0.1, 0.4);
                // Task *task_list[] = {&servo_task};
                // Task::run_tasks(task_list, 1);
            }
            move_servo(num.toFloat());
            delay(300);
            break;
        }
        Host->println("ok");
    }
    case 'M':
        switch (num)
        {
        case 110: // M110 重置行号
            args = line.substring(fs + 1);
            for (;;) // 读取目标位置
            {
                char c = args[0];
                int fs = args.indexOf(' ');
                if (c == 'N')
                    last_line_code = (fs == -1 ? args.substring(1) : args.substring(1, fs)).toInt();
                if (fs == -1)
                    break;
                args = args.substring(fs + 1);
            }
            break;
        case 114: // M114 获取当前位置
            position_status.Pos(x, y, M, N);
            Host->printf("// status: x%f y%f M%f N%f R%f m%d n%d\n",
                         x, y, M, N,
                         position_status.R,
                         position_status.m,
                         position_status.n);
            break;
        case 115: // M115 设置坐标参数
            args = line.substring(fs + 1);
            for (;;) // 读取目标位置
            {
                char c = args[0];
                int fs = args.indexOf(' ');
                if (c == 'R')
                    position_status.R = args.substring(1, fs == -1 ? args.length() : fs).toFloat();
                else if (c == 'm')
                    position_status.m = args.substring(1, fs == -1 ? args.length() : fs).toInt();
                else if (c == 'n')
                    position_status.n = args.substring(1, fs == -1 ? args.length() : fs).toInt();
                if (fs == -1)
                    break;
                args = args.substring(fs + 1);
            }
            break;
        case 401:
            save_settings();
            break;
        }
        break;
    }
}

void loop()
{
    // 从串口接受指令
    if (Serial.available())
        Host = &Serial;
#ifdef BLUETOOTH
    else if (SerialBT.available())
        Host = &SerialBT;
#endif
    else
        return;

    String cmd = Host->readStringUntil('\n'); // 读取一行
    if (!cmd.length())
        return;

    bool checked = cmd[0] == 'N';

    cmd = cmd.substring(0, cmd.indexOf(';')); // 去除注释
    int32_t line_code;
    if (!checkCMD(cmd, line_code))
        Host->printf("rs %d error: check code error\n", line_code);
    else if (checked && line_code != last_line_code + 1) // 行号不对，要求重发
        Host->printf("rs %d error: line number is not current line + 1. last line: %d\n",
                     last_line_code + 1,
                     last_line_code);
    else
    {
        exec_gcode(cmd);
        if (checked)
            last_line_code++;
    }
}
