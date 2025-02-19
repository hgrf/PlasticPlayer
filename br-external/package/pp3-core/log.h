#ifndef LOG_H
#define LOG_H

#define LOG_ERROR(MSG, ...) do { printf("<3>%s: " MSG "\n", TAG __VA_OPT__(,) __VA_ARGS__); } while(0)
#define LOG_WARNING(MSG, ...) do { printf("<4>%s: " MSG "\n", TAG __VA_OPT__(,) __VA_ARGS__); } while(0)
#define LOG_INFO(MSG, ...) do { printf("<6>%s: " MSG "\n", TAG __VA_OPT__(,) __VA_ARGS__); } while(0)

#endif // LOG_H
