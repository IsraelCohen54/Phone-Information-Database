#pragma once

#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <chrono>

#include "numbers.h"
#include "strings.h"
#include "CircularQueue_mt.h"
#include "StreamInOutDirection_mt.h"

namespace experis
{
namespace UI // static class
{

// "public" method:
void StartUI(StreamInOutDirection_mt& a_stream, CyclicBlockingQueue<std::vector<std::string>, 10>& a_queue);

} // namespace UI

} // experis namespace