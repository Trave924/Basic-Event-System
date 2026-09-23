//#include <pch.h>
#include <Events/Events.h>
#include <string>
#include <chrono>


std::atomic<bool> isRunning = true;


bool test() {
	return true;
}
bool test2(int te) {
	return true;
}

//void OldSpeed() {
//	EventHandler event;
//	std::cout << "========================================= OLD TEST ===================================" << std::endl;
//	int EVENT_COUNT = 10000;
//
//	std::atomic<int> value = 0;
//	std::atomic<int> callbackCount = 0;
//	auto start = std::chrono::high_resolution_clock::now();
//	for (int i = 0; i < EVENT_COUNT; ++i)
//	{
//		event.On("OldEvent_" + std::to_string(i), When(value, ET::EqualTo(1)), Increment(callbackCount));
//	}
//	auto end = std::chrono::high_resolution_clock::now();
//	auto registrationTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//
//	std::cout << "Registration time: " << registrationTime << " ms" << std::endl;
//	value = 1;
//
//	auto triggerStart = std::chrono::high_resolution_clock::now();
//
//	value = 1;
//	while (callbackCount.load() < EVENT_COUNT) {
//		event.HandleJobs();
//	}
//
//	auto triggerEnd = std::chrono::high_resolution_clock::now();
//	auto triggerTime = std::chrono::duration_cast<std::chrono::milliseconds>(triggerEnd - triggerStart).count();
//
//	std::cout << "Trigger time: " << triggerTime << " ms" << std::endl;
//	std::cout << "Callbacks: " << callbackCount.load() << std::endl;
//}
//
//void NewSpeed() {
//	EventHandler event;
//	std::cout << "========================================= NEW TEST ===================================" << std::endl;
//	int EVENT_COUNT = 100000;
//
//	std::atomic<int> value = 0;
//	std::atomic<int> callbackCount = 0;
//	auto start = std::chrono::high_resolution_clock::now();
//	event.BeginBatch();
//	for (int i = 0; i < EVENT_COUNT; ++i)
//	{
//		event.On("NewEvent_" + std::to_string(i), When(value, ET::EqualTo(1)), Increment(callbackCount));
//	}
//	event.EndBatch();
//	auto end = std::chrono::high_resolution_clock::now();
//	auto registrationTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//
//	std::cout << "Registration time: " << registrationTime << " ms" << std::endl;
//	value = 1;
//
//	auto triggerStart = std::chrono::high_resolution_clock::now();
//
//	value = 1;
//	while (callbackCount.load() < EVENT_COUNT) {
//		event.HandleJobs();
//	}
//
//	auto triggerEnd = std::chrono::high_resolution_clock::now();
//	auto triggerTime = std::chrono::duration_cast<std::chrono::milliseconds>(triggerEnd - triggerStart).count();
//
//	std::cout << "Trigger time: " << triggerTime << " ms" << std::endl;
//	std::cout << "Callbacks: " << callbackCount.load() << std::endl;
//}

void FullTest() {
	EventHandler event;

	int playerHealth = 100;
	bool playerDied = false;
	int deathCount = 0;

	//event.On("PlayerHealthZero", When(playerHealth, ET::LessOrEqualThan(0)), Set(playerDied, true), Set(playerDied, false));
	//event.On("PlayerDied", When(playerDied), Print("Player Died"));

	////event.While("isPlayerAlive", When(playerDied, ET::NotEqualTo(true)), Print("Player is Alive"));
	//event.On("DeathCount", When(playerDied), Do(Increment(deathCount), Print("Death Count : ", deathCount)));

	//event.On("LowHealth", When(playerHealth, ET::LessOrEqualThan(50)), Print("Low Health"));
	//event.On("CriticalHealth", When(playerHealth, ET::LessOrEqualThan(20)), Print("Critical Health"));
	//event.On("Dead", When(playerHealth, ET::LessOrEqualThan(0)), Print("Dead"));

	//event.On("Test", When(test), Print("Test"));
	//event.On("Testt", When([]() {return test2(10); }), Print("Test"));

	//int frames = 0;

	//bool timedTrigger = false;
	//event.While("timedEvent", When(true), Do(Print("timed event"), Set(timedTrigger, true)), nullptr, Timed(3.f));

	//event.On("sequencedEvent1", When(timedTrigger), Print("Sequenced Event 1"))
	//	.ThenOn("sequencedEvent2", When(true), Print("Sequenced Event 2"), nullptr , Timed(5.f))
	//	.ThenOn("sequencedEvent3", When(true), Print("Sequenced Event 3"), nullptr , Timed(3.f));

	//OldSpeed();
	//NewSpeed();

	//float test = 3.f;
	//float test2 = 1.3f;

	//event.On("test", When(true), Print("Test"), nullptr, Timed(4.f), false);
	//event.On("test2", When(true), Print("Test2"), nullptr, Timed([&]() {return test - test2; }), false);
	//event.On("test3", When(true), Print("Test3"), nullptr, Timed(test), false);

	EventValue<bool> testValue = false;
	EventAtomicValue<bool> testValue2 = false;
	event.On("testValue", When(testValue), Print("test"));
	event.On("testValue2", When(testValue2), Print("test2"));
	testValue = true;
	testValue2.store(true);

	while (isRunning.load()) {
		event.HandleJobs();

		//std::cout << testValue << std::endl;

		/*test2 += 0.01f;

		if (frames == 40)
			playerHealth = 50;
		else if (frames == 50)
			playerHealth = 20;
		else if (frames == 60)
			playerHealth = 0;

		frames++;*/
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

}

int main() {
	FullTest();
	//bool eventTriggered = true;
	//int eventTriggered2 = 10;


	//EventHandler event;
	//event.Once("event1", When(eventTriggered), Print("testt"));
	//event.Once("event2", When(eventTriggered2, ET::GreaterThan(20)), Print("testt2"));

	//std::unordered_map<int,int> healthes;
	//
	//for (int i = 0; i <= 50; i++) {
	//	healthes[i] = 100;
	//	event.Once("player: " + std::to_string(i), When(healthes[i],ET::EqualTo(0)), Print("player: ", i, " died"));
	//}

	//event.Once("Test", When(eventTriggered), Do(test,test,test));

	//bool changed = true;
	//event.OnChange("test3", Change(changed), Print("value changed : ", changed));

	////changed = false;

	//int frames = 0;
	//event.Once("framesCount2", When(frames, ET::GreaterThan(50)), Do(Toggle(changed)));
	//event.Once("framesCount", When(frames, ET::GreaterThan(100)), Do(Set(eventTriggered,true), Set(eventTriggered2, 30), Set(healthes[20],0), Set(healthes[28],0) , Toggle(changed)));
	//event.Once("framesCount3", When(frames, ET::GreaterThan(250)), Do(Toggle(changed)));

	//event.Once("Test2", When(isRunning), Print("zz"));

	//

	//while (isRunning.load()) {
	//	event.HandleJobs();

	//	frames++;
	//	std::this_thread::sleep_for(std::chrono::milliseconds(16));
	//}


	//return 0;
}