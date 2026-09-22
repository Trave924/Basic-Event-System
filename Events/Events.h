#pragma once
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <chrono>

using Event = std::function<void()>;

enum EventType {
	ET_Trigger,
	ET_OneTime,
	ET_Continuous,
	ET_OnChange,
};

struct Trigger {
	virtual ~Trigger() = default;
	virtual bool IsTriggered() = 0;
};

template<typename T>
class BoolTrigger : public Trigger {
	T& value;

public:
	BoolTrigger(T& value) : value(value) {}

	bool IsTriggered() override {
		return value;
	}
};


template<typename F>
class BoolFunctionTrigger : public Trigger {
	F function;

public:
	BoolFunctionTrigger(F&& function) : function(std::forward<F>(function)) {}

	bool IsTriggered() override {
		return function();
	}
};

template<typename T>
class staticTrigger : public Trigger {
	T value;
public:
	staticTrigger(T&& value) : value(value) {}

	bool IsTriggered() override {
		return value;
	}
};

template<typename T>
T& ReadValue(T& value) {
	return value;
}

template<typename T>
T ReadValue(std::atomic<T>& value) {
	return value.load();
}

template<typename T>
using TriggerValue = decltype(ReadValue(std::declval<T&>()));

template<typename T>
class FunctionTrigger : public Trigger {
	std::function<T()>& value;
	std::function<bool(TriggerValue<T>)> condition;
public:
	FunctionTrigger(std::function<T()> value, std::function<bool(TriggerValue<T>)> condition) : value(value), condition(std::move(condition)) {}

	bool IsTriggered() override {
		return condition(ReadValue(value()));
	}
};

template<typename T>
class ValueTrigger : public Trigger {
	T& value;
	std::function<bool(TriggerValue<T>)> condition;
public:
	ValueTrigger(T& value, std::function<bool(TriggerValue<T>)> condition) : value(value), condition(std::move(condition)) {}

	bool IsTriggered() override {
		return condition(ReadValue(value));
	}
};

template<typename T>
class ChangeTrigger : public Trigger {
	T& value;
	T previousValue;

public:
	ChangeTrigger(T& value) : value(value), previousValue(value) {}

	bool IsTriggered() override {
		if (value != previousValue) {
			previousValue = value;
			return true;
		}

		return false;
	}
};

struct EventState {
	bool wasTriggered = false;
	bool initialized = false;
	float time = 0;
	float timer = 0;

	bool active = true;
	std::string nextEvent;
};

struct EventData {
	std::string name;
	EventType type;
	std::shared_ptr<Trigger> trigger;
	Event callback;
	Event elseCallback = nullptr;
	float time = 0;
	float timer = 0;

};

template<typename T>
inline auto When(T& trigger, std::function<bool(TriggerValue<T>)> condition) {
	return std::make_shared<ValueTrigger<T>>(trigger, condition);
}
inline auto When(bool& trigger) {
	return std::make_shared<BoolTrigger<bool>>(trigger);
}
inline auto When(std::atomic<bool>& trigger) {
	return std::make_shared<BoolTrigger<std::atomic<bool>>>(trigger);
}

template<typename F>
requires std::invocable<F>
inline auto When(F&& trigger) {
	return std::make_shared<BoolFunctionTrigger<F>>(std::forward<F>(trigger));
}
template<typename T>
inline auto When(std::function<T()> trigger, std::function<bool(TriggerValue<T>)> condition) {
	return std::make_shared<FunctionTrigger<T>>(trigger, condition);
}
template<typename T>
inline auto When(T&& trigger) {
	return std::make_shared<staticTrigger<T>>(std::move(trigger));
}

template<typename T>
inline auto Change(T& value) {
	return std::make_shared<ChangeTrigger<T>>(value);
}

template<typename T>
inline Event Set(T& value, T newValue) {
	return [&value, newValue]() {value = newValue; };
}

template<typename T>
inline Event Toggle(T& value) {
	return [&value]() {value = !value; };
}

template<typename...Args>
inline Event Do(Args&&... args) {
	return [args...]() {(args(), ...); };
}

template<typename...Args>
inline Event Print(Args&&... args) {
	return [&args...]() {(std::cout << ... << args) << std::endl; };
}

template<typename T>
Event Increment(T& value) {
	return [&value]() {value++; };
}

namespace ET {
	template<typename T>
	std::function<bool(T)> GreaterThan(T triggerer) {
		return [triggerer](T value) {return value > triggerer; };
	}
	template<typename T>
	std::function<bool(T)> LessThan(T triggerer) {
		return [triggerer](T value) {return value < triggerer; };
	}
	template<typename T>
	std::function<bool(T)> EqualTo(T triggerer) {
		return [triggerer](T value) {return value == triggerer; };
	}
	template<typename T>
	std::function<bool(T)> NotEqualTo(T triggerer) {
		return [triggerer](T value) {return value != triggerer; };
	}
	template<typename T>
	std::function<bool(T)> GreaterOrEqualThan(T triggerer) {
		return [triggerer](T value) {return value >= triggerer; };
	}
	template<typename T>
	std::function<bool(T)> LessOrEqualThan(T triggerer) {
		return [triggerer](T value) {return value <= triggerer; };
	}
}

template<typename T>
class Snapshot {
	std::atomic<int> version = 0;
	std::atomic<std::shared_ptr<T>> events;
public:
	Snapshot() {
		events.store(std::make_shared<T>());
	}

	template< typename... Args>
	void Insert(Args&&... args) {
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);
		newData->insert_or_assign(std::forward<Args>(args)...);
		events.store(newData);
		version.fetch_add(1);
	}

	void Publish(const T& data)
	{
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);

		for (const auto& item : data)
			newData->insert_or_assign(item.first, item.second);

		events.store(newData);
		version.fetch_add(1);
	}

	template<typename... Args>
	void Erase(Args&&... args) {
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);

		newData->erase(std::forward<Args>(args)...);

		events.store(newData);
		version.fetch_add(1);
	}

	int Version() const { return version.load(); }

	std::shared_ptr<T> Get() {
		return events.load();
	}


};

class EventHandler;

class EventBuilder {
	EventHandler& handler;
	std::string eventName;
public:
	EventBuilder(EventHandler& handler, std::string eventName) : handler(handler), eventName(eventName) {}

	EventBuilder& ThenOnce(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0);
	EventBuilder& ThenOn(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0);
	EventBuilder& ThenOnChange(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0);
	EventBuilder& ThenWhile(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0);
};

class EventHandler {
	friend class EventBuilder;


	Snapshot<std::unordered_map<std::string, EventData>> eventsSnapshot;
	std::thread eventsThread;
	std::unordered_map<std::string, EventData> events;
	int dataVersion = 0;
	int jobVersion = 0;
	std::queue<Event> eventsJob;
	std::mutex jobsMutex;
	std::unordered_map<std::string, EventState> eventStates;
	bool batchBegined = false;
	std::unordered_map<std::string, EventData> batch;

	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point lastTime;
	float DeltaTime = 0.f;
	float Tick = 0.f;
public:
	std::atomic<bool> isEventsRunning = true;

	EventHandler() {
		eventsThread = std::thread([&]() {
			startTime = std::chrono::steady_clock::now();

			while (isEventsRunning.load()) {
				auto nowTime = std::chrono::steady_clock::now();
				DeltaTime = std::chrono::duration<float>(nowTime - lastTime).count() / 1000.f;
				Tick = std::chrono::duration<float>(nowTime - startTime).count();

				lastTime = std::chrono::steady_clock::now();

				int currentVersion = eventsSnapshot.Version();
				if (dataVersion != currentVersion) {
					events = *(eventsSnapshot.Get());
					dataVersion = currentVersion;

					for (auto& event : events) {
						eventStates[event.first].time = event.second.time;
						eventStates[event.first].timer = event.second.timer;
					}
				}

				for (auto& event : events) {
					auto& state = eventStates[event.first];

					if (Tick - state.timer < 0)
						continue;

					if (!state.active)
						continue;

					if (state.time > 0) {
						state.timer += state.time;
					}
					

					switch (event.second.type)
					{
					case ET_Trigger:
					{
						bool triggered = event.second.trigger->IsTriggered();

						if (triggered && !state.wasTriggered) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active){
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + nextState.time;
							}
							
							{
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.callback);
							}
						}
						else if (!triggered && eventStates[event.first].wasTriggered) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + nextState.time;
							}

							if (event.second.elseCallback) {
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.elseCallback);
							}
						}

						state.wasTriggered = triggered;
					}
					break;
					case ET_OneTime:
						if ((*event.second.trigger).IsTriggered()) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + nextState.time;
							}

							{
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.callback);
							}
							eventsSnapshot.Erase(event.first);
						}
						break;

					case ET_Continuous:
					{
						if (event.second.trigger->IsTriggered()) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + nextState.time;
							}

							std::lock_guard lock(jobsMutex);
							eventsJob.push(event.second.callback);
						}
					}
					break;
					case ET_OnChange:
					{
						if (event.second.trigger->IsTriggered()) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + nextState.time;
							}

							std::lock_guard lock(jobsMutex);
							eventsJob.push(event.second.callback);
						}
					}
					break;
					default:
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
			});
	}

	~EventHandler() {
		isEventsRunning = false;
		eventsThread.join();
	}
	EventBuilder Once(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent, timer, timer });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent, timer, timer });
	
		return EventBuilder(*this, eventName);
	}
	EventBuilder On(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent, timer, timer });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent, timer, timer });

		return EventBuilder(*this, eventName);
	}
	EventBuilder OnChange(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent, timer, timer });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent, timer, timer });
	
		return EventBuilder(*this, eventName);
	}
	EventBuilder While(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, float timer = 0) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent, timer, timer });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent, timer, timer });
		
		return EventBuilder(*this, eventName);
	}

	void BeginBatch() {
		batch.clear();
		batchBegined = true;
	}
	void EndBatch() {
		eventsSnapshot.Publish(batch);
		batchBegined = false;
	}

	void HandleJobs() {
		std::queue<Event> localJobs;
		{
			std::lock_guard lock(jobsMutex);
			std::swap(localJobs, eventsJob);
		}

		while (!localJobs.empty()) {
			auto& event = localJobs.front();

			event();

			localJobs.pop();
		}
	}
};

EventBuilder& EventBuilder::ThenOnce(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent, float timer) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_OneTime, trigger, event, elseEvent, timer, timer });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_OneTime, trigger, event, elseEvent, timer, timer });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenOn(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent , float timer ) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_Trigger, trigger, event, elseEvent, timer, timer });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_Trigger, trigger, event, elseEvent, timer, timer });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenOnChange(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent , float timer ) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_OnChange, trigger, event, elseEvent, timer, timer });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_OnChange, trigger, event, elseEvent, timer, timer });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenWhile(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent , float timer) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_Continuous, trigger, event, elseEvent, timer, timer });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_Continuous, trigger, event, elseEvent, timer, timer });

	eventName = nextEventName;
	return *this;
}