#include "console.h"

static String s_line = "";
static bool   s_has  = false;

// 从 Serial 读一行 (非阻塞, 无数据时立即返回)
// 若已暂存 line 则不重复读
void console_pump(void)
{
    if (s_has) return;
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
        s_line = line;
        s_has  = true;
    }
}

// 取走当前暂存的 line, 取走后 s_has=false
String console_take(void)
{
    if (!s_has) return "";
    s_has = false;
    String out = s_line;
    s_line = "";
    return out;
}

// 把不匹配的 line 退回暂存, 下个 console_* 再取
void console_give_back(const String& line)
{
    s_line = line;
    s_has  = true;
}
