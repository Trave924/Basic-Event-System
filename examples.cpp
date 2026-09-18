//#include <pch.h>
#include <Events/Events.h>
#include <string>


std::atomic<bool> isRunning = true;


void test() {
	std::cout << "test" << std::endl;
}

int main() {
	bool eventTriggered = true;
	int eventTriggered2 = 10;


	EventHandler event;
	event.Once("event1", When(eventTriggered), Print("testt"));
	event.Once("event2", When(eventTriggered2, ET::GreaterThan(20)), Print("testt2"));

	
	std::unordered_map<int,int> healthes;
	for (int i = 0; i <= 50; i++) {
		healthes[i] = 100;
		event.Once("player: " + std::to_string(i), When(healthes[i],ET::EqualTo(0)), Print("player: ", i, " died"));
	}

	event.Once("Test", When(eventTriggered), Do(test,test,test));

	bool changed = true;
	event.OnChange("test3", Change(changed), Print("value changed : ", changed));

	//changed = false;

	int frames = 0;
	event.Once("framesCount2", When(frames, ET::GreaterThan(50)), Do(Toggle(changed)));
	event.Once("framesCount", When(frames, ET::GreaterThan(100)), Do(Set(eventTriggered,true), Set(eventTriggered2, 30), Set(healthes[20],0), Set(healthes[28],0) , Toggle(changed)));
	event.Once("framesCount3", When(frames, ET::GreaterThan(250)), Do(Toggle(changed)));

	event.Once("Test2", When(isRunning), Print("zz"));

	

	while (isRunning.load()) {
		event.HandleJobs();

		frames++;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}


	return 0;
}