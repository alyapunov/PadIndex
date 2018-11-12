#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif
#include <assert.h>

class CTimer {
public:
	CTimer(bool start = false) : started(false), result(0)
	{
		if (start)
			Start();
#ifdef _WIN32
		QueryPerformanceFrequency(&pf);
#endif
	}
	void Start()
	{
		started = true;
		startTime = now();
	}
	void Stop()
	{
		assert(started);
		started = false;
		result = now() - startTime;
	}
	long ElapsedMicrosec() const
	{
		if (started)
			return now() - startTime;
		else
			return result;
	}
	long ElapsedMillisec() const
	{
		return ElapsedMicrosec() / 1000;
	}
	long ElapsedSec() const
	{
		return ElapsedMicrosec() / 1000000;
	}
	double Elapsed() const
	{
		return double(ElapsedMicrosec()) / 1000000;
	}
	double Mrps(long count) const
	{
		return double(count) / ElapsedMicrosec();
	}
	double krps(long count) const
	{
		return Mrps(count) * 1000;
	}
	double rps(long count) const
	{
		return Mrps(count) * 1000000;
	}
	long ElapsedMicroSec() const { return ElapsedMicrosec(); }
	long ElapsedMicroseconds() const { return ElapsedMicrosec(); }
	long ElapsedMicroSeconds() const { return ElapsedMicrosec(); }
	long ElapsedMilliSec() const { return ElapsedMillisec(); }
	long ElapsedMilliseconds() const { return ElapsedMillisec(); }
	long ElapsedMilliSeconds() const { return ElapsedMillisec(); }
	long ElapsedSeconds() const { return ElapsedSec(); }

private:
	bool started;
	unsigned long long result;
	unsigned long long startTime;
#ifdef _WIN32
	LARGE_INTEGER pf;
#endif
	unsigned long long now() const
	{
#ifdef _WIN32
		LARGE_INTEGER pc;
		QueryPerformanceCounter(&pc);
		return (unsigned long long)(1000000 * double(pc.QuadPart) / double(pf.QuadPart));
#else
		struct timeval tv;
		gettimeofday(&tv, 0);
		return ((unsigned long long)tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
	}
};
