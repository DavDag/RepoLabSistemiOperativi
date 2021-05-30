#pragma once

#ifndef COMMON_H
#define COMMON_H

#define RES_OK 0
#define RES_ERROR -1

#include "utils.h"
#include "uuid.h"
#include "net.h"
#include "murmurhash2.h"

// Just for better logging
#define BYTES(b) ((b>1024*1024)?((float)b)/1024/1024:(b>1024)?((float)b)/1024:b),((b>1024*1024)?"MB":(b>1024)?"KB":" B")

#endif // COMMON_H
