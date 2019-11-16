#pragma once
#include<functional>
#include<windows.h>
#define ON_K_EVENT (WM_USER|0X5224)
class KRunnable
{
public:
	bool is_timer = false;
	typedef std::function<void()> Runnable;
	Runnable run;
	KRunnable(Runnable func);
	void runTask();
	void runTaskLater(long time);
	void runTaskAsync();
	void runTaskLaterAsync(long time);
	void runTaskTimer(long first, long interval);
	void runTaskTimerAsync(long first, long interval);
};

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void createMessageWindow();