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

enum EventCommandType {
	EVENT_ACTIVATE,
	EVENT_DEACTIVATE,
	EVENT_REMOVE
};

struct EventCommand {
	EventCommandType type;
	std::string eventName;
};

template<typename T>
class EventValue {
	std::shared_ptr<T> value;
public:
	EventValue(T val) {
		value = std::make_shared<T>(val);
	}

	auto Get() {
		return value;
	}

	operator T& () {
		return *value;
	}

	EventValue& operator=(T val) {
		*value = val;
		return *this;
	}
};

template<typename T>
class EventAtomicValue {
	std::shared_ptr<std::atomic<T>> value;
public:
	EventAtomicValue(T val) {
		value = std::make_shared<std::atomic<T>>(val);
	}

	auto Get() {
		return value;
	}

	T load() const {
		return value->load();
	}

	void store(T val) {
		value->store(val);
	}

	T exchange(T val) {
		return value->exchange(val);
	}

	T fetch_add(T val) {
		return value->fetch_add(val);
	}

	T fetch_sub(T val) {
		return value->fetch_sub(val);
	}

	EventAtomicValue& operator=(T val) {
		store(val);
		return *this;
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

struct Trigger {
	virtual ~Trigger() = default;
	virtual bool IsTriggered() = 0;
};

template<typename T>
class BoolTrigger : public Trigger {
	std::weak_ptr<T> value;

public:
	BoolTrigger(std::shared_ptr<T> value) : value(value) {}

	bool IsTriggered() override {
		if (auto obj = value.lock()) {
			return ReadValue(*obj);
		}
		else {
			// logic to destroy event later
			std::cout << "value destroyed" << std::endl;
			return false;
		}
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
	std::weak_ptr<T> value;
	std::function<bool(TriggerValue<T>)> condition;
public:
	ValueTrigger(std::shared_ptr<T> value, std::function<bool(TriggerValue<T>)> condition) : value(value), condition(std::move(condition)) {}

	bool IsTriggered() override {
		if (auto obj = value.lock()) {
			return condition(ReadValue(*obj));
		}
		else {
			// logic to destroy event later
			std::cout << "value destroyed" << std::endl;
			return false;
		}
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
	std::function<float()> time = 0;
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
	std::function<float()> time = 0;
	float timer = 0;
	bool executeImmediately = false;
};


inline auto When(EventValue<bool> trigger) {
	return std::make_shared<BoolTrigger<bool>>(trigger.Get());
}

// (NOT LIFE TIME AWARE) on your risk , i couldn't reach values ref in lambda and make it api beginner friendly at the same time 
template<typename F>
requires std::invocable<F>
inline auto When(F&& trigger) {
	return std::make_shared<BoolFunctionTrigger<F>>(std::forward<F>(trigger));
}

template<typename T>
inline auto When(EventValue<T> trigger, std::function<bool(TriggerValue<T>)> condition) {
	return std::make_shared<ValueTrigger<T>>(trigger.Get(), condition);
}

inline auto When(EventAtomicValue<bool> trigger) {
	return std::make_shared<BoolTrigger<std::atomic<bool>>>(trigger.Get());
}

//template<typename F>
//	requires std::invocable<F>
//inline auto When(F&& trigger) {
//	return std::make_shared<BoolFunctionTrigger<F>>(std::forward<F>(trigger));
//}
//template<typename T>
//inline auto When(std::function<T()> trigger, std::function<bool(TriggerValue<T>)> condition) {
//	return std::make_shared<FunctionTrigger<T>>(trigger, condition);
//}
//template<typename T>
//inline auto When(T&& trigger) {
//	return std::make_shared<staticTrigger<T>>(std::move(trigger));
//}

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

template <typename F>
inline auto Timed(F&& time) {
	return std::function<float()>(std::forward<F>(time));
}

inline auto Timed(float&& time) {
	return [time]() {return time; };
}

inline auto Timed(float& time) {
	return [time]() {return time; };
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

	void EraseSome(std::vector<std::string> names) {
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);

		for (auto& name : names)
			newData->erase(name);

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

	EventBuilder& ThenOnce(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false);
	EventBuilder& ThenOn(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false);
	EventBuilder& ThenOnChange(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false);
	EventBuilder& ThenWhile(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false);
};

class EventHandler {
	friend class EventBuilder;


	Snapshot<std::unordered_map<std::string, EventData>> eventsSnapshot;
	std::thread eventsThread;
	std::unordered_map<std::string, EventData> events;
	int dataVersion = 0;
	int jobVersion = 0;
	std::queue<Event> eventsJob;
	std::queue<EventCommand> outerCommandsJob;
	std::mutex jobsMutex;
	std::mutex commandsMutex;
	std::unordered_map<std::string, EventState> eventStates;
	bool batchBegined = false;
	std::unordered_map<std::string, EventData> batch;
	std::vector<std::string> removeBatch;

	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point lastTime;
	float DeltaTime = 0.f;
	float Tick = 0.f;
public:
	std::atomic<bool> isEventsRunning = true;

	EventHandler() {
		eventsThread = std::thread([&]() {
			startTime = std::chrono::steady_clock::now();
			lastTime = startTime;

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

				std::queue<EventCommand> localCommands;
				{ std::lock_guard lock(commandsMutex); std::swap(localCommands, outerCommandsJob); }
				while (!localCommands.empty()) {
					auto& command = localCommands.front();

					switch (command.type)
					{
					case EVENT_ACTIVATE:
						eventStates[command.eventName].active = true;
						break;
					case EVENT_DEACTIVATE:
						eventStates[command.eventName].active = false;
						break;
					case EVENT_REMOVE:
						removeBatch.push_back(command.eventName);
						eventStates.erase(command.eventName);
						break;
					default:
						break;
					}

					localCommands.pop();
				}

				if (removeBatch.size() > 0)
					eventsSnapshot.EraseSome(removeBatch);
				removeBatch.clear();

				for (auto& event : events) {
					auto& state = eventStates[event.first];

					if (Tick - state.timer < 0 && !(event.second.executeImmediately && !state.initialized))
						continue;

					if (!state.active)
						continue;

					float time = state.time();

					if (time > 0 && state.initialized)
						state.timer += time;

					if (!state.initialized)
						state.initialized = true;



					switch (event.second.type)
					{
					case ET_Trigger:
					{
						bool triggered = event.second.trigger->IsTriggered();

						if (triggered && !state.wasTriggered) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + time;
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
								nextState.timer = Tick + time;
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
								nextState.timer = Tick + time;
							}

							{
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.callback);
							}
							eventsSnapshot.Erase(event.first);
							eventStates.erase(event.first);
						}
						break;

					case ET_Continuous:
					{
						if (event.second.trigger->IsTriggered()) {
							if (state.nextEvent != "" && !eventStates[state.nextEvent].active) {
								auto& nextState = eventStates[state.nextEvent];
								nextState.active = true;
								nextState.timer = Tick + time;
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
								nextState.timer = Tick + time;
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
	EventBuilder Once(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent, timer, timer(), executeImmediately });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent, timer, timer(), executeImmediately });

		return EventBuilder(*this, eventName);
	}
	EventBuilder On(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent, timer, timer(), executeImmediately });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent, timer, timer(), executeImmediately });

		return EventBuilder(*this, eventName);
	}
	EventBuilder OnChange(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent, timer, timer(), executeImmediately });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent, timer, timer(), executeImmediately });

		return EventBuilder(*this, eventName);
	}
	EventBuilder While(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr, std::function<float()> timer = Timed(0.f), bool executeImmediately = false) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent, timer, timer() ,executeImmediately });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent, timer, timer(), executeImmediately });

		return EventBuilder(*this, eventName);
	}

	void Activate(std::string eventName) {
		std::lock_guard lock(commandsMutex);
		outerCommandsJob.push({ EVENT_ACTIVATE,eventName });
	}

	void DeActivate(std::string eventName) {
		std::lock_guard lock(commandsMutex);
		outerCommandsJob.push({ EVENT_DEACTIVATE,eventName });
	}

	void Remove(std::string eventName) {
		std::lock_guard lock(commandsMutex);
		outerCommandsJob.push({ EVENT_REMOVE,eventName });
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

EventBuilder& EventBuilder::ThenOnce(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent, std::function<float()> timer, bool executeImmediately) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_OneTime, trigger, event, elseEvent, timer, timer(), executeImmediately });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_OneTime, trigger, event, elseEvent, timer, timer(), executeImmediately });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenOn(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent, std::function<float()> timer, bool executeImmediately) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_Trigger, trigger, event, elseEvent, timer, timer(), executeImmediately });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_Trigger, trigger, event, elseEvent, timer, timer(), executeImmediately });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenOnChange(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent, std::function<float()> timer, bool executeImmediately) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_OnChange, trigger, event, elseEvent, timer, timer(), executeImmediately });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_OnChange, trigger, event, elseEvent, timer, timer(), executeImmediately });

	eventName = nextEventName;
	return *this;
}

EventBuilder& EventBuilder::ThenWhile(std::string nextEventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent, std::function<float()> timer, bool executeImmediately) {
	handler.eventStates[nextEventName].active = false;
	handler.eventStates[eventName].nextEvent = nextEventName;
	if (!handler.batchBegined)
		handler.eventsSnapshot.Insert(nextEventName, EventData{ nextEventName, ET_Continuous, trigger, event, elseEvent, timer, timer(), executeImmediately });
	else
		handler.batch.insert_or_assign(nextEventName, EventData{ nextEventName, ET_Continuous, trigger, event, elseEvent, timer, timer(), executeImmediately });

	eventName = nextEventName;
	return *this;
}