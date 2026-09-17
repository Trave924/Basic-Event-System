//#include <pch.h>
#include <Events/Events.h>
#include <string>


std::atomic<bool> isRunning = true;


int main() {
	bool eventTriggered = true;
	int eventTriggered2 = 10;


	EventHandler eventHandler;
	eventHandler.Once("event1", When(eventTriggered), Print("testt"));
	eventHandler.Once("event2", When(eventTriggered2, ET::GreaterThan(20)), Print("testt2"));

	
	std::unordered_map<int,int> healthes;
	for (int i = 0; i <= 50; i++) {
		healthes[i] = 100;
		eventHandler.Once("player: " + std::to_string(i), When(healthes[i],ET::EqualTo(0)), Print("player: ", i, " died"));
	}

	int frames = 0;
	eventHandler.Once("framesCount", When(frames, ET::GreaterThan(100)), Do(Set(eventTriggered,true), Set(eventTriggered2, 30), Set(healthes[20],0), Set(healthes[28],0)));


	while (isRunning.load()) {
		eventHandler.HandleJobs();


		frames++;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}


	return 0;
}