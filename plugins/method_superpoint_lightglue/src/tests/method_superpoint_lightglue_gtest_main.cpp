#include "runtime/testing/PluginGTestMain.hpp"

int main(int argc, char **argv)
{
    return PoSDK::Testing::runPluginGTest(
        argc, argv, QStringLiteral("method_superpoint_lightglue"),
        PoSDK::PluginRuntime::PluginKind::Method);
}
