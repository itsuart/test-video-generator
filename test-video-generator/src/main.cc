extern "C" {
#include <libavcodec/avcodec.h>
}


int wmain(int /*argc*/, const wchar_t** /*argv*/) {
    avcodec_register_all();
    return 0;
}