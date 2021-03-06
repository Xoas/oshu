/**
 * \file lib/core/log.cc
 * \ingroup core_log
 */

#include "core/log.h"

#include <iostream>

namespace oshu {
inline namespace core {
namespace log {

level priority = level::warning;

level& operator++(level &l)
{
	if (l == level::critical)
		return l;
	int raw = static_cast<int>(l);
	l = static_cast<level>(raw + 1);
	return l;
}

level& operator--(level &l)
{
	if (l == level::verbose)
		return l;
	int raw = static_cast<int>(l);
	l = static_cast<level>(raw - 1);
	return l;
}

/**
 * Dummy output stream.
 */
static std::ostream devnull {nullptr};

std::ostream& logger(level priority)
{
	if (priority >= oshu::core::log::priority)
		return std::clog;
	else
		return devnull;
}

std::ostream& verbose()  { return logger(level::verbose)  << "VERBOSE: "; }
std::ostream& debug()    { return logger(level::debug)    << "DEBUG: "; }
std::ostream& info()     { return logger(level::info)     << "INFO: "; }
std::ostream& warning()  { return logger(level::warning)  << "WARNING: "; }
std::ostream& error()    { return logger(level::error)    << "ERROR: "; }
std::ostream& critical() { return logger(level::critical) << "CRITICAL: "; }

}}}
