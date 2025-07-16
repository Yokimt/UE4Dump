#include "dfm.h"

int main()
{
    g_dumpInfo.InitOffsets();
    if(g_dumpInfo.InitDriver)
    {
        g_dumpInfo.DumpSDK("/data/local/tmp/dfm");
    }
    return 0;
}