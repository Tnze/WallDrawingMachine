// 检查N代码是否正确，并去除它
bool checkCMD(String &cmd, int &line_code)
{
    // 如果没有校验则直接返回true
    if (cmd[0] != 'N')
        return true;

    int start = cmd.indexOf(' ');
    int end = cmd.lastIndexOf('*');

    if (start == -1 || end == -1)
        return false; // 格式不正确

    // 计算码字
    line_code = cmd.substring(1, start).toInt();    // 行码
    int chek_code = cmd.substring(end + 1).toInt(); // 校验码

    // 计算本地校验码
    int cs = 0;
    for (int i = 0; i != end; i++)
        cs = cs ^ cmd[i];
    cs &= 0xff; // Defensive programming...

    // 去除原字符串校验部分
    cmd = cmd.substring(start + 1, end);

    return cs == chek_code;
}
