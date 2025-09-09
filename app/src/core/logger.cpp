#include "logger.h"
#include <raylib.h>
#include <vector>

#include "core/fmemory.h"

#define LOGGING_SEVERITY LOG_SEV_INFO
#define LOG_FILE_LOCATION ""
#define LOG_FILE_NAME "log.txt"

typedef struct logging_system_state {
  std::vector<std::string> logs;
} logging_system_state;

static logging_system_state * state = nullptr;

bool logging_system_initialize(void) {
  if (state and state != nullptr) {
    return false;
  }
  state = (logging_system_state*)allocate_memory_linear(sizeof(logging_system_state), true);
  if (not state or state == nullptr) {
    return false;
  }

  const char * path = TextFormat("%s%s",LOG_FILE_LOCATION, LOG_FILE_NAME);
  if (FileExists(path)) {
    SaveFileText(path, " ");
  }

  return true;
}

void logging_system_shutdown(void) {
  if (not state or state == nullptr) {
    return;
  }
  for (std::string str : state->logs) {
		str.clear();
		str.shrink_to_fit();
	}
  state->logs.clear();
	state->logs.shrink_to_fit();
	
  free(state);
  state = nullptr;
}

void inc_logging(logging_severity ls, const char* fmt, ...) {
  if(not state or state == nullptr) {
		return;
  }
  std::string out_log = std::string();
  char timeStr[64] = { 0 };
  time_t now = time(NULL);

  struct tm *tm_info = localtime(&now);
  strftime(timeStr, sizeof(timeStr), "[%Y-%m-%d %H:%M:%S", tm_info);
  out_log.append(timeStr);

  switch (ls)
  {
    case LOG_SEV_INFO : out_log.append("::INFO]: "); break;
    case LOG_SEV_WARNING : out_log.append("::WARN]: "); break;
    case LOG_SEV_ERROR : out_log.append("::ERROR]: "); break;
    case LOG_SEV_FATAL : out_log.append("::FATAL]: "); break;
    default: break;
  }
  __builtin_va_list arg_ptr;
  va_start(arg_ptr, fmt); 

  // https://stackoverflow.com/questions/19009094/c-variable-arguments-with-stdstring-only
  // reliably acquire the size from a copy of
  // the variable argument array
  // and a functionally reliable call
  // to mock the formatting
  va_list arg_ptr_copy;
  va_copy(arg_ptr_copy, arg_ptr);
  const int iLen = std::vsnprintf(NULL, 0, fmt, arg_ptr_copy);
  va_end(arg_ptr_copy);

  // return a formatted string without
  // risking memory mismanagement
  // and without assuming any compiler
  // or platform specific behavior
  std::vector<char> zc(iLen + 1);
  std::vsnprintf(zc.data(), zc.size(), fmt, arg_ptr);
  va_end(arg_ptr);

  out_log.append(std::string(zc.data(), zc.size()-1)).append("\n");
	std::string& log = state->logs.emplace_back(out_log);

	if (ls >= LOGGING_SEVERITY) {
		const char * path = TextFormat("%s%s",LOG_FILE_LOCATION, LOG_FILE_NAME);
    if (FileExists(path)) {
      char * logged_text = LoadFileText(path);
      SaveFileText(path, TextFormat("%s%s", logged_text, log.c_str()));
      UnloadFileText(logged_text);
    }
    else {
      SaveFileText(path, TextFormat("%s", log.c_str()));
    }
	}
}
