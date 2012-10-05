#include <node.h>
#include <node_buffer.h>

#include <stdint.h>
#include <sstream>
#include <cstring>
#include <string>

#include "macros.h"
#include "blobber.h"

using namespace node_sqlite3;

namespace {

void RegisterModule(v8::Handle<Object> target) {
    Foozle::Init(target);
}

}

NODE_MODULE(node_sqlite3, RegisterModule);
