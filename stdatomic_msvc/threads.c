#include "threads.h"
#include<Windows.h>

int thrd_current()
{
	return GetCurrentThreadId();
}
