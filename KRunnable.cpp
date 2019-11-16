#include "KRunnable.h"
#include <thread>
static ATOM WindowClass;
HWND hwnd;
static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg == ON_K_EVENT) {
		//prevent all exceptions
		KRunnable* kr = ((KRunnable*)lp);
		__try {
			kr->run();
		}
		__except (1) {

		}
		__try {
			if(!kr->is_timer)
			delete kr;
		}
		__except (1) {}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
void createMessageWindow() {
	HINSTANCE hinst = ::GetModuleHandle(NULL);
	if (!WindowClass) {
		//make a window class
		WNDCLASSEXW wcex = {
			/*size*/sizeof(WNDCLASSEX), /*style*/0, /*proc*/WndProc, /*extra*/0L,0L, /*hinst*/hinst,
			/*icon*/NULL, /*cursor*/NULL, /*brush*/NULL, /*menu*/NULL,
			/*class*/L"KHttpServer Msg wnd class", /*smicon*/NULL };
		WindowClass = ::RegisterClassExW(&wcex);
		if (!WindowClass)
			throw L"register window class failed.";
	}
	//create a window for this instance to receive messahes
	hwnd = ::CreateWindowExW(0, (LPCWSTR)MAKELONG(WindowClass, 0), L"KRunnableMsg",
		0, 0, 0, 1, 1, HWND_MESSAGE, NULL, hinst, NULL);
	if (!hwnd) throw "create message window failed.";
}

KRunnable::KRunnable(Runnable func) :run(func) {}

void KRunnable::runTask()
{
	if (!hwnd)
		createMessageWindow();
	PostMessageW(hwnd, ON_K_EVENT, NULL, (LPARAM)this);
}

void KRunnable::runTaskLater(long time)
{
	std::thread th([this,time] {
		std::this_thread::sleep_for(std::chrono::milliseconds(time));
		runTask();
	});
	th.detach();
}

void KRunnable::runTaskAsync()
{
	std::thread th(run);
	th.detach();
}

void KRunnable::runTaskLaterAsync(long time)
{
	std::thread th([this, time] {
		std::this_thread::sleep_for(std::chrono::milliseconds(time));
		run();});
	th.detach();
}

void KRunnable::runTaskTimer(long first, long interval)
{
	std::thread th([this,first,interval] {
		std::this_thread::sleep_for(std::chrono::milliseconds(first));
		while (is_timer) {
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			runTask();
		}
		});
	th.detach();
}

void KRunnable::runTaskTimerAsync(long first, long interval)
{
	std::thread th([this, first, interval] {
		std::this_thread::sleep_for(std::chrono::milliseconds(first));
		while (is_timer) {
			std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			run();
		}
		});
	th.detach();
}
