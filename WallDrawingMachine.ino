#include <Stepper.h>
#include <stdint.h>
#include "coordinate.hpp"
#include "gcode.hpp"
#include "tasker.hpp"
// #define BLUETOOTH

#ifdef BLUETOOTH
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;
#endif

#define SPEED 500

Stepper stepperR(64, 32, 33, 25, 26);
Stepper stepperL(64, 19, 18, 5, 17);

void setup()
{
#ifdef BLUETOOTH
    SerialBT.begin(BLUETOOTH);
#endif
    Serial.begin(115200);
    stepperR.setSpeed(1000);
    stepperL.setSpeed(1000);
}

Stream *Host;
PositionStatus position_status(35.0f * PI / 2048.0f, 300, 5587, 5587);
int32_t last_line_code = 0; // 串口行号

void exec_gcode(String line)
{
    Host->println("// received: " + line);
    String args;
    if (line.length() < 1)
        return;
    int first_space = line.indexOf(' ');
    int num = line.substring(1, first_space).toInt();

    float x = 0, y = 0;
    float M, N;
    int m = 0, n = 0;

    switch (line[0])
    {
    case 'G':
        args = line.substring(first_space + 1);

        for (;;) // 读取目标位置
        {
            char c = args[0];
            int fs = args.indexOf(' ');
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
        Host->printf("// move steps: (m%d, n%d)\n", m, n);
        { // Move (m, n)
            unsigned long duration = sqrtl(m * m + n * n) * 1000 / SPEED;
            MotorTask left_task(stepperL, 0, duration, true, m);
            MotorTask right_task(stepperR, 0, duration, true, n);
            Task *task_list[] = {&left_task, &right_task};
            Task::run_tasks(task_list, 2);
        }
        Host->println("ok");
        break;

    case 'M':
        switch (num)
        {
        case 110: // M110 重置行号
            args = line.substring(first_space + 1);
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
            Host->printf("x%f y%f M%f N%f R%f m%d n%d\n",
                         x, y, M, N,
                         position_status.R,
                         position_status.m,
                         position_status.n);
            break;
        case 115: // M115 设置坐标参数
            args = line.substring(first_space + 1);
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
